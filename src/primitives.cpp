#include "mgv/primitives.hpp"

#include <algorithm>
#include <cmath>
#include <numbers>
#include <stdexcept>

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
        .tex_coord = {surface.green, surface.approach},
    };
}

} // namespace

SurfaceClass course_surface_class(
    float x,
    float z,
    const CourseTerrainDescription& description) noexcept {
    const auto half = description.green_half_extent;

    // The putting surface, with a collar of fringe around its edge.
    const auto radius = std::sqrt(x * x + z * z);
    const auto green = 1.0F - smoothstep_between(half * 0.74F, half * 1.0F, radius);

    // The approach runs away from the green down -Z, narrowing slightly as it
    // goes, with mown edges rather than a hard boundary against the rough.
    const auto along = -z;
    const auto taper = 1.0F - 0.25F * std::clamp(along / description.approach_length, 0.0F, 1.0F);
    const auto half_width = description.approach_half_width * taper;
    const auto across = 1.0F - smoothstep_between(half_width * 0.72F, half_width, std::abs(x));
    const auto run = smoothstep_between(-half * 1.3F, half * 0.2F, along) *
                     (1.0F - smoothstep_between(
                         description.approach_length * 0.82F,
                         description.approach_length,
                         along));
    const auto approach = std::clamp(across * run, 0.0F, 1.0F);

    return {.green = std::clamp(green, 0.0F, 1.0F), .approach = std::max(approach, green)};
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

} // namespace mgv
