#pragma once

#include "mgv/mesh.hpp"

namespace mgv {

/// Builds an inward-facing hemisphere-plus-skirt used as a sky dome.
///
/// The skirt continues a little below the horizon so a camera tilted down at
/// the ball still has sky geometry behind distant terrain. Normals point
/// inwards, towards the eye at the centre.
[[nodiscard]] MeshData make_sky_dome(float radius, int rings, int sectors);

/// The shape of a hole: a tightly tessellated putting surface surrounded by
/// progressively coarser rings of landscape running out to the horizon.
struct CourseTerrainDescription final {
    /// Half the width of the mown green, in metres.
    float green_half_extent{7.62F};
    /// Quads per side across the green. Its undulation is around a tenth of a
    /// metre, so this has to stay fine.
    int green_resolution{100};
    /// Half the width of the whole terrain, in metres.
    float outer_extent{420.0F};
    /// Concentric rings between the green and the outer edge. Each is a fixed
    /// fraction wider than the last, so triangle density falls off with
    /// distance while staying fine enough near the player that the ground
    /// does not visibly facet.
    int rings{52};
    /// Half-width of the mown approach running away from the green, in metres.
    float approach_half_width{17.0F};
    /// How far the approach runs before it gives way to rough, in metres.
    float approach_length{190.0F};
};

/// How a point on the course is cut, stored per vertex.
///
/// Surface class belongs to the course, not to the shape of the ground: a
/// bunker and a green can both be dead level. Carrying it on the mesh means
/// the same authoring survives when hand-built holes replace generated ones.
struct SurfaceClass final {
    /// 1 on the putting surface, falling to 0 across the collar.
    float green{};
    /// 1 on the mown approach, falling to 0 into the rough.
    float approach{};
};

[[nodiscard]] SurfaceClass course_surface_class(
    float x,
    float z,
    const CourseTerrainDescription& description) noexcept;

/// The terrain height at a point, in metres.
///
/// Inside the green this is exactly the analytic surface the physics
/// heightmap uses, so what the ball rolls on and what is drawn cannot drift
/// apart. Beyond it the same surface continues, with rolling landscape faded
/// in, which keeps the join seamless in both value and slope.
[[nodiscard]] float course_terrain_height(
    float x,
    float z,
    const CourseTerrainDescription& description) noexcept;

/// Builds the hole described by `description` as a single watertight mesh.
[[nodiscard]] MeshData make_course_terrain(const CourseTerrainDescription& description);

/// Builds a tapered cylinder standing on the origin along +Y, capped at both
/// ends. `tex_coord.y` runs 0 at the base to 1 at the top.
[[nodiscard]] MeshData make_cylinder(
    float bottom_radius,
    float top_radius,
    float height,
    int sides);

/// Builds a cone standing on the origin along +Y.
[[nodiscard]] MeshData make_cone(float radius, float height, int sides);

/// Builds the flag end of a pin: a plain rectangle hanging from the stick,
/// given a gentle curl so it does not read as a flat sheet of card.
[[nodiscard]] MeshData make_flag(float width, float height, int segments);

/// A broadleaf tree, as one mesh.
///
/// `tex_coord.x` marks material: 0 for bark, 1 for canopy, so a single shader
/// can light both without a second draw call per tree.
struct TreeDescription final {
    float height{9.0F};
    float trunk_radius{0.26F};
    /// Stacked canopy masses, which read far better than one ball of leaves.
    int canopy_lobes{5};
    float canopy_radius{3.4F};
    /// Seed for the per-tree variation in lobe placement and size.
    unsigned int seed{1u};
};

[[nodiscard]] MeshData make_tree(const TreeDescription& description);

/// Builds a flat, subdivided square in the XZ plane, centred on the origin and
/// facing +Y. Subdivision exists so that per-vertex work and large triangles
/// do not interpolate badly across the ground.
[[nodiscard]] MeshData make_ground_plane(float size, int subdivisions);

} // namespace mgv
