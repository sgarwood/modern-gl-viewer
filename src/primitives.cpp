#include "mgv/primitives.hpp"

#include <algorithm>
#include <cmath>
#include <numbers>
#include <stdexcept>
#include <vector>

namespace mgv {
namespace {

constexpr float pi = std::numbers::pi_v<float>;

} // namespace

MeshData make_sky_dome(float radius, int rings, int sectors) {
    if (!(radius > 0.0F)) {
        throw std::invalid_argument{"A sky dome needs a positive radius"};
    }
    if (rings < 2 || sectors < 3) {
        throw std::invalid_argument{"A sky dome needs at least 2 rings and 3 sectors"};
    }

    // Run from a little past the horizon up to the zenith.
    constexpr float lowest_elevation = -0.18F * pi;
    const float highest_elevation = 0.5F * pi;

    MeshData mesh;
    mesh.vertices.reserve(static_cast<std::size_t>((rings + 1) * (sectors + 1)));
    for (int ring = 0; ring <= rings; ++ring) {
        const auto v = static_cast<float>(ring) / static_cast<float>(rings);
        const auto elevation = lowest_elevation + v * (highest_elevation - lowest_elevation);
        const auto y = std::sin(elevation);
        const auto horizontal = std::cos(elevation);
        for (int sector = 0; sector <= sectors; ++sector) {
            const auto u = static_cast<float>(sector) / static_cast<float>(sectors);
            const auto azimuth = u * 2.0F * pi;
            const Vec3 direction{
                horizontal * std::cos(azimuth),
                y,
                horizontal * std::sin(azimuth),
            };
            mesh.vertices.push_back({
                .position = {direction.x * radius, direction.y * radius, direction.z * radius},
                .normal = {-direction.x, -direction.y, -direction.z},
                .tex_coord = {u, v},
            });
        }
    }

    const auto stride = static_cast<std::uint32_t>(sectors + 1);
    mesh.indices.reserve(static_cast<std::size_t>(rings * sectors * 6));
    for (int ring = 0; ring < rings; ++ring) {
        for (int sector = 0; sector < sectors; ++sector) {
            const auto row = static_cast<std::uint32_t>(ring) * stride;
            const auto next_row = row + stride;
            const auto column = static_cast<std::uint32_t>(sector);
            // Wound so the inward-facing side is the front face.
            mesh.indices.insert(mesh.indices.end(), {
                row + column,
                next_row + column,
                row + column + 1,
                row + column + 1,
                next_row + column,
                next_row + column + 1,
            });
        }
    }
    return mesh;
}

MeshData make_ground_plane(float size, int subdivisions) {
    if (!(size > 0.0F)) {
        throw std::invalid_argument{"A ground plane needs a positive size"};
    }
    if (subdivisions < 1) {
        throw std::invalid_argument{"A ground plane needs at least one subdivision"};
    }

    const auto half = size * 0.5F;
    const auto step = size / static_cast<float>(subdivisions);

    MeshData mesh;
    mesh.vertices.reserve(static_cast<std::size_t>((subdivisions + 1) * (subdivisions + 1)));
    for (int row = 0; row <= subdivisions; ++row) {
        for (int column = 0; column <= subdivisions; ++column) {
            const auto x = -half + static_cast<float>(column) * step;
            const auto z = -half + static_cast<float>(row) * step;
            mesh.vertices.push_back({
                .position = {x, 0.0F, z},
                .normal = {0.0F, 1.0F, 0.0F},
                .tex_coord = {
                    static_cast<float>(column) / static_cast<float>(subdivisions),
                    static_cast<float>(row) / static_cast<float>(subdivisions),
                },
            });
        }
    }

    const auto stride = static_cast<std::uint32_t>(subdivisions + 1);
    mesh.indices.reserve(static_cast<std::size_t>(subdivisions * subdivisions * 6));
    for (int row = 0; row < subdivisions; ++row) {
        for (int column = 0; column < subdivisions; ++column) {
            const auto base = static_cast<std::uint32_t>(row) * stride +
                              static_cast<std::uint32_t>(column);
            mesh.indices.insert(mesh.indices.end(), {
                base,
                base + stride,
                base + 1,
                base + 1,
                base + stride,
                base + stride + 1,
            });
        }
    }
    return mesh;
}


namespace {

/// The mown surface: a steady fall towards -Z with a gentle cross-undulation.
/// This is the surface `tools/generate_green.py` bakes and the physics
/// heightmap samples, reproduced exactly.
[[nodiscard]] float green_surface(float x, float z) noexcept {
    return -0.05F * z + 0.1F * std::sin(0.4F * z) * std::cos(0.4F * x);
}

/// Landscape beyond the green. Low frequency and incommensurate so it does not
/// read as a repeating pattern from the tee.
[[nodiscard]] float surrounding_landscape(float x, float z) noexcept {
    return 2.1F * std::sin(x * 0.0121F) * std::cos(z * 0.0094F) +
           1.3F * std::sin(x * 0.0313F + 1.7F) * std::sin(z * 0.0271F - 0.6F) +
           0.45F * std::sin(x * 0.0742F - 2.2F) * std::cos(z * 0.0688F + 0.9F);
}

[[nodiscard]] float smoothstep_between(float edge0, float edge1, float value) noexcept {
    if (edge1 <= edge0) {
        return value < edge0 ? 0.0F : 1.0F;
    }
    const auto t = std::clamp((value - edge0) / (edge1 - edge0), 0.0F, 1.0F);
    return t * t * (3.0F - 2.0F * t);
}

[[nodiscard]] Vec3 terrain_normal(float x, float z, const CourseTerrainDescription& d) noexcept {
    constexpr float step = 0.02F;
    const auto dx = course_terrain_height(x + step, z, d) - course_terrain_height(x - step, z, d);
    const auto dz = course_terrain_height(x, z + step, d) - course_terrain_height(x, z - step, d);
    const Vec3 normal{-dx, 2.0F * step, -dz};
    const auto length = std::sqrt(normal.x * normal.x + normal.y * normal.y + normal.z * normal.z);
    return {normal.x / length, normal.y / length, normal.z / length};
}

[[nodiscard]] Vertex terrain_vertex(float x, float z, const CourseTerrainDescription& d) {
    const auto surface = course_surface_class(x, z, d);
    return {
        .position = {x, course_terrain_height(x, z, d), z},
        .normal = terrain_normal(x, z, d),
        // The turf shader derives everything else from world position, so the
        // texture coordinate carries the surface class instead.
        .tex_coord = {surface.cut_height, surface.mow},
    };
}

} // namespace

SurfaceClass course_surface_class(
    float x,
    float z,
    const CourseTerrainDescription& description) noexcept {
    const auto half = description.green_half_extent;

    // The putting surface, ringed by a collar of fringe. The collar is a
    // genuine third cut, not a blend artefact: it is what gives a green its
    // crisp edge instead of letting it dissolve into the approach.
    const auto radius = std::sqrt(x * x + z * z);
    // A mower changes height abruptly, so these edges are crisp. Blending a
    // green into its surroundings over several metres is what made the three
    // cuts indistinguishable in the first place.
    const auto green = 1.0F - smoothstep_between(half * 0.90F, half * 0.965F, radius);
    // A collar wide enough to read from the fairway: roughly a metre and a
    // half of unmown fringe, which is about what a greenkeeper leaves.
    const auto collar = 1.0F - smoothstep_between(half * 1.14F, half * 1.21F, radius);

    // The approach runs away from the green down -Z, narrowing slightly as it
    // goes, with mown edges rather than a hard boundary against the rough.
    const auto along = -z;
    const auto taper = 1.0F - 0.25F * std::clamp(along / description.approach_length, 0.0F, 1.0F);
    const auto half_width = description.approach_half_width * taper;
    const auto across = 1.0F - smoothstep_between(half_width * 0.93F, half_width, std::abs(x));
    const auto run = smoothstep_between(-half * 1.4F, half * 0.4F, along) *
                     (1.0F - smoothstep_between(
                         description.approach_length * 0.82F,
                         description.approach_length,
                         along));
    const auto approach = std::clamp(across * run, 0.0F, 1.0F);

    constexpr float green_cut = 0.0032F;
    constexpr float collar_cut = 0.0092F;
    constexpr float fairway_cut = 0.0135F;
    constexpr float rough_cut = 0.062F;

    auto cut = rough_cut;
    cut = std::lerp(cut, fairway_cut, approach);
    cut = std::lerp(cut, collar_cut, collar);
    cut = std::lerp(cut, green_cut, green);

    // The collar sits between the two patterns and is mown in neither, so the
    // signal passes through zero there on its own.
    const auto mow = std::lerp(approach, -1.0F, green) * (1.0F - collar * (1.0F - green));

    return {.cut_height = cut, .mow = std::clamp(mow, -1.0F, 1.0F)};
}

float course_terrain_height(
    float x,
    float z,
    const CourseTerrainDescription& description) noexcept {
    const auto base = green_surface(x, z);
    // Radial, not the larger of the two axes: taking the maximum would give
    // the landscape square iso-contours, and square contours around the green
    // are glaringly obvious once there is any directional light on them.
    const auto radius = std::sqrt(x * x + z * z);
    // The landscape may not start until past the corners of the square green,
    // or the drawn surface would diverge from the physics heightmap exactly
    // where the two are hardest to reconcile. Hence the diagonal, not the
    // half-extent.
    constexpr auto diagonal = 1.4142136F;
    const auto blend = smoothstep_between(
        description.green_half_extent * diagonal,
        description.green_half_extent * 6.5F,
        radius);
    return base + blend * surrounding_landscape(x, z);
}

MeshData make_course_terrain(const CourseTerrainDescription& description) {
    if (!(description.green_half_extent > 0.0F)) {
        throw std::invalid_argument{"A course needs a positive green extent"};
    }
    if (description.green_resolution < 1) {
        throw std::invalid_argument{"A course needs at least one quad across the green"};
    }
    if (description.outer_extent <= description.green_half_extent) {
        throw std::invalid_argument{"A course must extend beyond its green"};
    }
    if (description.rings < 1) {
        throw std::invalid_argument{"A course needs at least one surrounding ring"};
    }

    const auto resolution = description.green_resolution;
    const auto per_side = resolution;  // vertices along one edge of a ring, minus the corner
    MeshData mesh;

    // --- the green itself ---------------------------------------------------
    const auto green_half = description.green_half_extent;
    const auto green_step = (green_half * 2.0F) / static_cast<float>(resolution);
    for (int row = 0; row <= resolution; ++row) {
        for (int column = 0; column <= resolution; ++column) {
            mesh.vertices.push_back(terrain_vertex(
                -green_half + static_cast<float>(column) * green_step,
                -green_half + static_cast<float>(row) * green_step,
                description));
        }
    }
    const auto green_stride = static_cast<std::uint32_t>(resolution + 1);
    for (int row = 0; row < resolution; ++row) {
        for (int column = 0; column < resolution; ++column) {
            const auto base = static_cast<std::uint32_t>(row) * green_stride +
                              static_cast<std::uint32_t>(column);
            mesh.indices.insert(mesh.indices.end(), {
                base, base + green_stride, base + 1,
                base + 1, base + green_stride, base + green_stride + 1,
            });
        }
    }

    // --- surrounding rings --------------------------------------------------
    // Each ring is a square loop of vertices. Consecutive loops have the same
    // vertex count, so they can be stitched by index without any T-junctions,
    // while their spacing grows geometrically and the triangles get larger the
    // further they are from the player.
    const auto loop_vertices = static_cast<std::uint32_t>(4 * per_side);
    const auto growth = std::pow(
        description.outer_extent / green_half,
        1.0F / static_cast<float>(description.rings));

    /// Appends one square loop at the given half-extent, walking the perimeter
    /// in a consistent direction so loops can be zipped together in order.
    const auto append_loop = [&](float half) {
        const auto step = (half * 2.0F) / static_cast<float>(per_side);
        for (int i = 0; i < per_side; ++i) {  // -X edge, running +Z
            mesh.vertices.push_back(
                terrain_vertex(-half, -half + static_cast<float>(i) * step, description));
        }
        for (int i = 0; i < per_side; ++i) {  // +Z edge, running +X
            mesh.vertices.push_back(
                terrain_vertex(-half + static_cast<float>(i) * step, half, description));
        }
        for (int i = 0; i < per_side; ++i) {  // +X edge, running -Z
            mesh.vertices.push_back(
                terrain_vertex(half, half - static_cast<float>(i) * step, description));
        }
        for (int i = 0; i < per_side; ++i) {  // -Z edge, running -X
            mesh.vertices.push_back(
                terrain_vertex(half - static_cast<float>(i) * step, -half, description));
        }
    };

    // The first loop sits on the green's own boundary, so the ring mesh begins
    // exactly where the green ends.
    auto previous_loop = static_cast<std::uint32_t>(mesh.vertices.size());
    append_loop(green_half);
    auto half = green_half;
    for (int ring = 0; ring < description.rings; ++ring) {
        half = ring + 1 == description.rings ? description.outer_extent : half * growth;
        const auto current_loop = static_cast<std::uint32_t>(mesh.vertices.size());
        append_loop(half);
        for (std::uint32_t i = 0; i < loop_vertices; ++i) {
            const auto next = (i + 1) % loop_vertices;
            const auto inner = previous_loop + i;
            const auto inner_next = previous_loop + next;
            const auto outer = current_loop + i;
            const auto outer_next = current_loop + next;
            mesh.indices.insert(mesh.indices.end(), {
                inner, outer, inner_next,
                inner_next, outer, outer_next,
            });
        }
        previous_loop = current_loop;
    }

    return mesh;
}


namespace {

/// Appends a radially symmetric surface of revolution given its profile.
///
/// Each profile entry is a radius and a height; consecutive entries are
/// joined into a band. Normals come from the profile's own slope, so a cone
/// and a cylinder both shade correctly without a special case.
void append_revolution(
    MeshData& mesh,
    const std::vector<Vec2>& profile,
    int sides,
    float material,
    Vec3 offset = {}) {
    if (profile.size() < 2 || sides < 3) {
        return;
    }
    const auto first_vertex = static_cast<std::uint32_t>(mesh.vertices.size());
    const auto stride = static_cast<std::uint32_t>(sides + 1);

    for (std::size_t level = 0; level < profile.size(); ++level) {
        const auto radius = profile[level].x;
        const auto height = profile[level].y;
        // Slope of the profile at this level, for the normal's vertical part.
        const auto previous = profile[level == 0 ? 0 : level - 1];
        const auto next = profile[std::min(level + 1, profile.size() - 1)];
        const auto run = next.x - previous.x;
        const auto rise = next.y - previous.y;
        const auto slope_length = std::sqrt(run * run + rise * rise);
        const auto normal_radial = slope_length > 1.0e-6F ? rise / slope_length : 1.0F;
        const auto normal_up = slope_length > 1.0e-6F ? -run / slope_length : 0.0F;

        for (int side = 0; side <= sides; ++side) {
            const auto angle = 2.0F * pi * static_cast<float>(side) / static_cast<float>(sides);
            const auto cosine = std::cos(angle);
            const auto sine = std::sin(angle);
            const Vec3 normal{normal_radial * cosine, normal_up, normal_radial * sine};
            const auto length =
                std::sqrt(normal.x * normal.x + normal.y * normal.y + normal.z * normal.z);
            mesh.vertices.push_back({
                .position = {offset.x + radius * cosine, offset.y + height, offset.z + radius * sine},
                .normal = length > 1.0e-6F
                    ? Vec3{normal.x / length, normal.y / length, normal.z / length}
                    : Vec3{0.0F, 1.0F, 0.0F},
                .tex_coord = {
                    material,
                    static_cast<float>(level) / static_cast<float>(profile.size() - 1),
                },
            });
        }
    }

    for (std::size_t level = 0; level + 1 < profile.size(); ++level) {
        for (int side = 0; side < sides; ++side) {
            const auto base = first_vertex + static_cast<std::uint32_t>(level) * stride +
                              static_cast<std::uint32_t>(side);
            mesh.indices.insert(mesh.indices.end(), {
                base, base + stride, base + 1,
                base + 1, base + stride, base + stride + 1,
            });
        }
    }
}

/// A small deterministic generator, so a given tree seed always produces the
/// same tree on every machine and in every run.
class Rng final {
public:
    explicit Rng(unsigned int seed) : state_{seed == 0u ? 1u : seed} {}

    [[nodiscard]] float next(float low, float high) {
        state_ = state_ * 1664525u + 1013904223u;
        const auto unit = static_cast<float>((state_ >> 8u) & 0xFFFFFFu) /
                          static_cast<float>(0x1000000u);
        return low + unit * (high - low);
    }

private:
    unsigned int state_{};
};

} // namespace

MeshData make_cylinder(float bottom_radius, float top_radius, float height, int sides) {
    if (height <= 0.0F || sides < 3) {
        throw std::invalid_argument{"A cylinder needs a positive height and at least 3 sides"};
    }
    MeshData mesh;
    append_revolution(mesh, {{0.0F, 0.0F}, {bottom_radius, 0.0F},
                             {top_radius, height}, {0.0F, height}}, sides, 0.0F);
    return mesh;
}

MeshData make_cone(float radius, float height, int sides) {
    if (radius <= 0.0F || height <= 0.0F || sides < 3) {
        throw std::invalid_argument{"A cone needs a positive radius, height, and 3 sides"};
    }
    MeshData mesh;
    append_revolution(mesh, {{0.0F, 0.0F}, {radius, 0.0F}, {0.0F, height}}, sides, 0.0F);
    return mesh;
}

MeshData make_flag(float width, float height, int segments) {
    if (width <= 0.0F || height <= 0.0F || segments < 1) {
        throw std::invalid_argument{"A flag needs a positive size and at least one segment"};
    }
    MeshData mesh;
    const auto stride = static_cast<std::uint32_t>(segments + 1);
    for (int row = 0; row <= 1; ++row) {
        for (int column = 0; column <= segments; ++column) {
            const auto u = static_cast<float>(column) / static_cast<float>(segments);
            // A shallow curl away from the stick, deepening along its length.
            const auto curl = std::sin(u * pi * 1.3F) * width * 0.16F;
            mesh.vertices.push_back({
                .position = {u * width, static_cast<float>(row) * height, curl},
                .normal = {0.0F, 0.0F, 1.0F},
                .tex_coord = {u, static_cast<float>(row)},
            });
        }
    }
    for (int column = 0; column < segments; ++column) {
        const auto base = static_cast<std::uint32_t>(column);
        mesh.indices.insert(mesh.indices.end(), {
            base, base + stride, base + 1,
            base + 1, base + stride, base + stride + 1,
        });
    }
    return mesh;
}

TreePalette tree_palette(TreeSpecies species) noexcept {
    switch (species) {
    case TreeSpecies::oak:
        // Deeply fissured grey-brown bark, and the darkest, bluest canopy of
        // the four.
        return {
            .bark = {0.055F, 0.045F, 0.036F},
            .leaf_shade = {0.026F, 0.058F, 0.021F},
            .leaf_sun = {0.070F, 0.123F, 0.036F},
        };
    case TreeSpecies::beech:
        // Famously smooth, pale grey bark against a bright mid-green crown.
        return {
            .bark = {0.112F, 0.106F, 0.094F},
            .leaf_shade = {0.038F, 0.072F, 0.026F},
            .leaf_sun = {0.112F, 0.158F, 0.049F},
        };
    case TreeSpecies::maple:
        // Warm reddish-brown bark, and the lightest, yellowest foliage.
        return {
            .bark = {0.058F, 0.042F, 0.032F},
            .leaf_shade = {0.044F, 0.078F, 0.027F},
            .leaf_sun = {0.137F, 0.165F, 0.048F},
        };
    case TreeSpecies::pine:
        // Red-brown plated bark under near-black blue-green needles.
        return {
            .bark = {0.052F, 0.031F, 0.021F},
            .leaf_shade = {0.015F, 0.033F, 0.022F},
            .leaf_sun = {0.040F, 0.073F, 0.042F},
        };
    }
    return {};
}

namespace {

/// Appends one canopy mass: a sphere of revolution, flattened or drawn out
/// vertically, centred where it is told, with a lumpy surface.
///
/// The lumps are the point. Smooth spheres, however many are overlapped,
/// still read as spheres, because the silhouette stays circular and the eye
/// reads silhouette first. The displacement is a smooth function of
/// direction rather than a per-vertex hash: hashing a low-poly sphere only
/// turns it into visible flat facets, which looks worse than the sphere did.
/// It costs nothing at runtime, since the mesh is built once and instanced.
void append_lobe(MeshData& mesh, Vec3 centre, float radius, float vertical, int sides, float phase) {
    constexpr int levels = 9;
    std::vector<Vec2> profile;
    profile.reserve(levels + 1);
    for (int level = 0; level <= levels; ++level) {
        const auto t = static_cast<float>(level) / static_cast<float>(levels);
        const auto polar = t * pi;
        profile.push_back({radius * std::sin(polar), -radius * vertical * std::cos(polar)});
    }

    const auto first = mesh.vertices.size();
    append_revolution(mesh, profile, sides, 1.0F, centre);
    for (auto index = first; index < mesh.vertices.size(); ++index) {
        auto& vertex = mesh.vertices[index];
        const auto& direction = vertex.normal;
        const auto lumps =
            0.55F * std::sin(4.1F * direction.x + phase) * std::cos(3.7F * direction.y - phase) +
            0.45F * std::sin(3.3F * direction.z + phase * 1.7F) * std::cos(4.9F * direction.x);
        const auto displacement = radius * 0.21F * lumps;
        vertex.position.x += direction.x * displacement;
        vertex.position.y += direction.y * displacement;
        vertex.position.z += direction.z * displacement;
    }
}

/// Appends a tapered tube between two points, used for limbs.
///
/// A trunk that rises and simply stops, with a ball of leaves balanced on
/// top, is the single thing that makes a generated tree look generated. Real
/// crowns are carried on limbs, and those limbs are visible through the
/// foliage from below and in winter silhouette.
void append_tapered_tube(
    MeshData& mesh,
    Vec3 from,
    Vec3 to,
    float start_radius,
    float end_radius,
    int sides) {
    const Vec3 axis{to.x - from.x, to.y - from.y, to.z - from.z};
    const auto length = std::sqrt(axis.x * axis.x + axis.y * axis.y + axis.z * axis.z);
    if (!(length > 1.0e-4F) || sides < 3) {
        return;
    }
    const Vec3 forward{axis.x / length, axis.y / length, axis.z / length};

    // Any vector not parallel to the limb will do to start the frame.
    const Vec3 reference = std::abs(forward.y) > 0.95F
        ? Vec3{1.0F, 0.0F, 0.0F}
        : Vec3{0.0F, 1.0F, 0.0F};
    Vec3 right{
        reference.y * forward.z - reference.z * forward.y,
        reference.z * forward.x - reference.x * forward.z,
        reference.x * forward.y - reference.y * forward.x,
    };
    const auto right_length = std::sqrt(right.x * right.x + right.y * right.y + right.z * right.z);
    right = {right.x / right_length, right.y / right_length, right.z / right_length};
    const Vec3 up{
        forward.y * right.z - forward.z * right.y,
        forward.z * right.x - forward.x * right.z,
        forward.x * right.y - forward.y * right.x,
    };

    const auto first = static_cast<std::uint32_t>(mesh.vertices.size());
    const auto stride = static_cast<std::uint32_t>(sides + 1);
    for (int end = 0; end <= 1; ++end) {
        const auto centre = end == 0 ? from : to;
        const auto radius = end == 0 ? start_radius : end_radius;
        for (int side = 0; side <= sides; ++side) {
            const auto angle = 2.0F * pi * static_cast<float>(side) / static_cast<float>(sides);
            const auto cosine = std::cos(angle);
            const auto sine = std::sin(angle);
            const Vec3 normal{
                right.x * cosine + up.x * sine,
                right.y * cosine + up.y * sine,
                right.z * cosine + up.z * sine,
            };
            mesh.vertices.push_back({
                .position = {
                    centre.x + normal.x * radius,
                    centre.y + normal.y * radius,
                    centre.z + normal.z * radius,
                },
                .normal = normal,
                .tex_coord = {0.0F, static_cast<float>(end)},
            });
        }
    }
    for (int side = 0; side < sides; ++side) {
        const auto base = first + static_cast<std::uint32_t>(side);
        mesh.indices.insert(mesh.indices.end(), {
            base, base + stride, base + 1,
            base + 1, base + stride, base + stride + 1,
        });
    }
}

/// How a species carries its crown.
struct CrownShape final {
    /// Height at which foliage begins, as a fraction of the tree's height.
    float base_fraction{};
    /// Crown half-width, as a fraction of the tree's height.
    float radius_fraction{};
    /// Above one the crown is taller than it is wide, below one flatter.
    float vertical{1.0F};
    int lobes{};
    int limbs{};
    /// How far the limbs reach out, as a fraction of the crown radius.
    float limb_reach{0.6F};
};

/// Builds a broadleaf crown: limbs rising out of the bole, and overlapping
/// masses of foliage filling the ellipsoid they reach into.
///
/// Lobes are distributed through the crown's volume and biased outwards,
/// because foliage grows where the light is. Filling it evenly gives a solid
/// ball; hanging a few lobes off the top gives a lollipop.
void append_broadleaf_crown(
    MeshData& mesh,
    Rng& rng,
    float height,
    float trunk_top,
    float trunk_radius,
    const CrownShape& crown) {
    const auto base = height * crown.base_fraction;
    const auto top = height * 0.97F;
    const auto centre_y = (base + top) * 0.5F;
    const auto half_height = (top - base) * 0.5F;
    const auto radius = height * crown.radius_fraction;

    for (int limb = 0; limb < crown.limbs; ++limb) {
        const auto angle = 2.0F * pi * static_cast<float>(limb) / static_cast<float>(crown.limbs) +
                           rng.next(-0.45F, 0.45F);
        const auto reach = radius * crown.limb_reach * rng.next(0.72F, 1.0F);
        const auto rise = rng.next(0.35F, 0.85F) * half_height;
        append_tapered_tube(
            mesh,
            {0.0F, trunk_top - height * 0.06F, 0.0F},
            {std::cos(angle) * reach, centre_y + rise * 0.35F, std::sin(angle) * reach},
            trunk_radius * 0.62F,
            trunk_radius * 0.20F,
            6);
    }

    for (int lobe = 0; lobe < crown.lobes; ++lobe) {
        // Bias outwards: the cube root of a uniform sample spreads points
        // evenly through a ball, and raising the exponent pushes them to the
        // shell where the leaves actually are.
        const auto reach = std::pow(rng.next(0.08F, 1.0F), 0.42F);
        const auto azimuth = rng.next(0.0F, 2.0F * pi);
        const auto elevation = std::asin(rng.next(-0.85F, 1.0F));
        const auto horizontal = std::cos(elevation);
        append_lobe(
            mesh,
            {
                std::cos(azimuth) * horizontal * reach * radius * 0.74F,
                centre_y + std::sin(elevation) * reach * half_height * 0.80F,
                std::sin(azimuth) * horizontal * reach * radius * 0.74F,
            },
            radius * rng.next(0.36F, 0.58F),
            crown.vertical,
            13,
            rng.next(0.0F, 6.28F));
    }
}

/// A conifer crown: whorls of branches narrowing to a leader, which is the
/// silhouette that tells a pine from everything else on the course.
void append_conifer_crown(
    MeshData& mesh,
    Rng& rng,
    float height,
    float bare_height,
    float crown_radius) {
    const auto crown_height = height - bare_height;
    constexpr int whorls = 8;
    for (int whorl = 0; whorl < whorls; ++whorl) {
        const auto t = static_cast<float>(whorl) / static_cast<float>(whorls - 1);
        // Whorls narrow towards the top, and the lowest is not the widest:
        // the bottom branches of a mature pine have already died back.
        const auto taper = std::pow(1.0F - t, 1.25F);
        const auto radius = crown_radius * (0.22F + 0.78F * taper) *
                            (t < 0.14F ? 0.80F : 1.0F) * rng.next(0.88F, 1.06F);
        const auto base = bare_height + t * crown_height * rng.next(0.96F, 1.04F);
        const auto span = crown_height / static_cast<float>(whorls) * 2.2F;

        // Each whorl is a shallow cone that droops: branches rise from the
        // trunk, then fall away under their own weight.
        append_revolution(
            mesh,
            {{0.0F, -span * 0.12F},
             {radius * 0.45F, -span * 0.24F},
             {radius * 0.85F, -span * 0.12F},
             {radius, span * 0.08F},
             {radius * 0.55F, span * 0.50F},
             {0.0F, span * 0.76F}},
            9,
            1.0F,
            {0.0F, base, 0.0F});
    }
    // The leader, so the tree comes to a point rather than a dome.
    append_revolution(
        mesh,
        {{0.0F, 0.0F}, {crown_radius * 0.20F, 0.0F}, {0.0F, crown_height * 0.18F}},
        7,
        1.0F,
        {0.0F, height - crown_height * 0.18F, 0.0F});
}

} // namespace

MeshData make_tree(const TreeDescription& description) {
    if (!(description.height > 0.0F)) {
        throw std::invalid_argument{"A tree needs a positive height"};
    }
    if (!(description.spread > 0.0F)) {
        throw std::invalid_argument{"A tree needs a positive spread"};
    }

    Rng rng{description.seed};
    const auto height = description.height;
    const auto spread = description.spread;
    MeshData mesh;

    // Proportions are the species. Crown half-width and the height the
    // foliage starts at identify an oak from a beech across a fairway long
    // before any difference in colour does.
    struct Profile final {
        float trunk_fraction;      // bole height, as a fraction of height
        float trunk_radius_scale;  // trunk radius, as a fraction of height
        float flare;               // how much the base swells
        CrownShape crown;
    };

    const auto profile = [&]() -> Profile {
        switch (description.species) {
        case TreeSpecies::oak:
            // A short, heavy bole dividing low into a crown wider than the
            // tree is tall, carried on a few massive limbs.
            return {0.34F, 0.052F, 1.80F,
                    {.base_fraction = 0.30F, .radius_fraction = 0.52F * spread,
                     .vertical = 0.68F, .lobes = 11, .limbs = 5, .limb_reach = 0.72F}};
        case TreeSpecies::beech:
            // A long, clean grey column carrying a crown held high, taller
            // than it is wide.
            return {0.46F, 0.034F, 1.45F,
                    {.base_fraction = 0.42F, .radius_fraction = 0.34F * spread,
                     .vertical = 1.22F, .lobes = 10, .limbs = 4, .limb_reach = 0.55F}};
        case TreeSpecies::maple:
            // A tidy, almost symmetrical round head on a medium trunk.
            return {0.38F, 0.038F, 1.50F,
                    {.base_fraction = 0.34F, .radius_fraction = 0.38F * spread,
                     .vertical = 0.98F, .lobes = 9, .limbs = 4, .limb_reach = 0.60F}};
        case TreeSpecies::pine:
            return {0.92F, 0.028F, 1.55F, {}};
        }
        return {};
    }();

    if (description.species == TreeSpecies::pine) {
        // A straight pole, bare for the lower third, in tiers to a point.
        const auto trunk_radius = height * profile.trunk_radius_scale;
        append_revolution(
            mesh,
            {{0.0F, 0.0F},
             {trunk_radius * profile.flare, 0.0F},
             {trunk_radius, height * 0.12F},
             {trunk_radius * 0.42F, height * 0.82F},
             {0.0F, height * 0.94F}},
            8,
            0.0F);
        append_conifer_crown(mesh, rng, height, height * 0.34F, height * 0.17F * spread);
        return mesh;
    }

    const auto trunk_top = height * profile.trunk_fraction;
    const auto trunk_radius = height * profile.trunk_radius_scale;
    append_revolution(
        mesh,
        {{0.0F, 0.0F},
         {trunk_radius * profile.flare, 0.0F},
         {trunk_radius * 1.12F, trunk_top * 0.20F},
         {trunk_radius * 0.94F, trunk_top * 0.70F},
         {trunk_radius * 0.78F, trunk_top},
         {0.0F, trunk_top * 1.04F}},
        9,
        0.0F);
    append_broadleaf_crown(mesh, rng, height, trunk_top, trunk_radius, profile.crown);
    return mesh;
}

MeshData make_leaf(int segments) {
    if (segments < 2) {
        throw std::invalid_argument{"A leaf needs at least two segments"};
    }

    // A pointed oval lying flat, pointing +Z, one unit long. Giving the leaf
    // its outline in geometry rather than in an alpha mask keeps it to a
    // dozen vertices and costs no texture fetch, which matters when there are
    // tens of thousands of them.
    MeshData mesh;
    mesh.vertices.push_back({
        .position = {0.0F, 0.0F, -0.5F},
        .normal = {0.0F, 1.0F, 0.0F},
        .tex_coord = {0.5F, 0.0F},
    });
    for (int segment = 1; segment < segments; ++segment) {
        const auto v = static_cast<float>(segment) / static_cast<float>(segments);
        // Widest a third of the way along, tapering to a point.
        const auto half_width = 0.26F * std::sin(std::pow(v, 0.72F) * pi);
        for (const auto side : {-1.0F, 1.0F}) {
            mesh.vertices.push_back({
                .position = {side * half_width, 0.0F, v - 0.5F},
                .normal = {0.0F, 1.0F, 0.0F},
                .tex_coord = {side * 0.5F + 0.5F, v},
            });
        }
    }
    mesh.vertices.push_back({
        .position = {0.0F, 0.0F, 0.5F},
        .normal = {0.0F, 1.0F, 0.0F},
        .tex_coord = {0.5F, 1.0F},
    });

    mesh.indices.insert(mesh.indices.end(), {0u, 1u, 2u});
    for (int segment = 1; segment + 1 < segments; ++segment) {
        const auto base = static_cast<std::uint32_t>(segment) * 2 - 1;
        mesh.indices.insert(mesh.indices.end(), {
            base, base + 2, base + 1,
            base + 1, base + 2, base + 3,
        });
    }
    const auto last = static_cast<std::uint32_t>(segments - 1) * 2 - 1;
    const auto tip = static_cast<std::uint32_t>(mesh.vertices.size() - 1);
    mesh.indices.insert(mesh.indices.end(), {last, tip, last + 1});
    return mesh;
}

MeshData make_leaf_litter(
    std::span<const LeafPile> piles,
    const LeafLitterDescription& litter,
    const CourseTerrainDescription& terrain) {
    if (!(litter.density > 0.0F)) {
        throw std::invalid_argument{"Leaf litter needs a positive density"};
    }
    if (!(litter.leaf_length > 0.0F)) {
        throw std::invalid_argument{"Leaf litter needs a positive leaf length"};
    }

    auto mesh = make_leaf(litter.segments);
    Rng rng{litter.seed};

    for (const auto& pile : piles) {
        if (!(pile.radius > 0.0F) || !(pile.depth > 0.0F)) {
            continue;
        }
        const auto count = static_cast<int>(pi * pile.radius * pile.radius * litter.density);
        for (int index = 0; index < count; ++index) {
            const auto angle = rng.next(0.0F, 2.0F * pi);
            const auto distance = pile.radius * std::sqrt(rng.next(0.0F, 1.0F));
            const auto x = pile.centre.x + std::cos(angle) * distance;
            const auto z = pile.centre.y + std::sin(angle) * distance;

            // The mound thins to nothing at the rim, so a drift has an edge
            // rather than a cliff.
            const auto reach = distance / pile.radius;
            const auto fall_off = 1.0F - reach * reach;
            const auto mound = pile.depth * fall_off * fall_off;
            if (!(mound > 1.0e-4F)) {
                continue;
            }
            // Thin the count towards the rim as well as the depth, or the
            // drift ends on a hard circle of leaves laid flat on the grass.
            if (rng.next(0.0F, 1.0F) > 1.0F - smoothstep_between(0.55F, 1.0F, reach)) {
                continue;
            }

            // Leaves fill the mound's volume rather than tiling its surface,
            // so something dropped into a drift is among them, not on them.
            const auto depth_fraction = rng.next(0.0F, 1.0F);
            const auto ground = course_terrain_height(x, z, terrain);

            mesh.instances.push_back({
                .position = {x, ground + mound * depth_fraction, z},
                .yaw = rng.next(0.0F, 2.0F * pi),
                .parameters = {
                    litter.leaf_length * rng.next(0.74F, 1.30F),
                    // Leaves lie mostly flat, but a few stand on edge against
                    // their neighbours, which is what gives a drift its
                    // broken, airy surface.
                    rng.next(-0.42F, 0.42F) +
                        (rng.next(0.0F, 1.0F) > 0.84F ? rng.next(-1.1F, 1.1F) : 0.0F),
                    rng.next(0.0F, 1.0F),
                    depth_fraction,
                },
            });
        }
    }

    if (mesh.instances.empty()) {
        mesh.instances.push_back({.parameters = {0.0F, 0.0F, 0.0F, 0.0F}});
    }
    return mesh;
}


MeshData make_grass_blade(int segments) {
    if (segments < 1) {
        throw std::invalid_argument{"A grass blade needs at least one segment"};
    }

    MeshData mesh;
    // Pairs of vertices up the blade, closing to a single vertex at the tip.
    for (int row = 0; row < segments; ++row) {
        const auto v = static_cast<float>(row) / static_cast<float>(segments);
        // The blade narrows towards the tip, quickly at first.
        const auto half_width = 0.5F * (1.0F - v * v * 0.55F - v * 0.35F);
        for (const auto side : {-1.0F, 1.0F}) {
            mesh.vertices.push_back({
                .position = {side * half_width, v, 0.0F},
                .normal = {0.0F, 0.0F, 1.0F},
                .tex_coord = {side * 0.5F + 0.5F, v},
            });
        }
    }
    mesh.vertices.push_back({
        .position = {0.0F, 1.0F, 0.0F},
        .normal = {0.0F, 0.0F, 1.0F},
        .tex_coord = {0.5F, 1.0F},
    });

    for (int row = 0; row + 1 < segments; ++row) {
        const auto base = static_cast<std::uint32_t>(row) * 2;
        mesh.indices.insert(mesh.indices.end(), {
            base, base + 1, base + 2,
            base + 2, base + 1, base + 3,
        });
    }
    const auto last = static_cast<std::uint32_t>(segments - 1) * 2;
    const auto tip = static_cast<std::uint32_t>(mesh.vertices.size() - 1);
    mesh.indices.insert(mesh.indices.end(), {last, last + 1, tip});
    return mesh;
}

MeshData make_grass_field(
    const GrassFieldDescription& field,
    const CourseTerrainDescription& terrain) {
    if (!(field.radius > 0.0F)) {
        throw std::invalid_argument{"A grass field needs a positive radius"};
    }
    if (!(field.density > 0.0F)) {
        throw std::invalid_argument{"A grass field needs a positive density"};
    }

    auto mesh = make_grass_blade(field.segments);

    Rng rng{field.seed};
    const auto area = pi * field.radius * field.radius;
    const auto attempts = static_cast<int>(area * field.density);
    mesh.instances.reserve(static_cast<std::size_t>(attempts));

    for (int attempt = 0; attempt < attempts; ++attempt) {
        // Uniform over the disc: the square root keeps the scatter from
        // bunching at the centre, which a naive radius would.
        const auto angle = rng.next(0.0F, 2.0F * pi);
        const auto distance = field.radius * std::sqrt(rng.next(0.0F, 1.0F));
        const auto x = field.centre.x + std::cos(angle) * distance;
        const auto z = field.centre.y + std::sin(angle) * distance;

        const auto surface = course_surface_class(x, z, terrain);
        if (surface.cut_height < field.minimum_cut) {
            continue;
        }

        // Thin the scatter where the grass is shorter, rather than carpeting
        // a fairway with blades nobody can resolve, and thin it again towards
        // the rim. Without the second, the field ends in a visible ring where
        // blades stop and only the turf surface remains.
        const auto rim = smoothstep_between(0.70F, 1.0F, distance / field.radius);
        // A fairway still wants a full mat of blades; they are simply much
        // shorter. Thinning in proportion to the cut would leave it bare.
        const auto keep = std::clamp(surface.cut_height / 0.022F, 0.25F, 1.0F) * (1.0F - rim);
        if (rng.next(0.0F, 1.0F) > keep) {
            continue;
        }

        // Blades stand a little above the height of cut and vary widely; a
        // uniform lawn of identical blades reads as carpet. They also shorten
        // into the rim, so the field thins in height as well as in number.
        const auto height =
            surface.cut_height * rng.next(0.80F, 1.55F) * (1.0F - rim * 0.55F);
        const auto width = std::clamp(height * 0.085F, 0.0012F, 0.006F);
        mesh.instances.push_back({
            .position = {x, course_terrain_height(x, z, terrain), z},
            .yaw = rng.next(0.0F, 2.0F * pi),
            .parameters = {
                height,
                width,
                rng.next(0.0F, 1.0F),
                // Longer grass flops further; a mown blade stands up.
                rng.next(0.10F, 0.55F) * std::clamp(surface.cut_height / 0.06F, 0.25F, 1.0F),
            },
        });
    }

    if (mesh.instances.empty()) {
        // An empty instance list would be drawn once at the origin. Nothing
        // grew here, so give the caller nothing to draw.
        mesh.instances.push_back({.parameters = {0.0F, 0.0F, 0.0F, 0.0F}});
    }
    return mesh;
}

} // namespace mgv
