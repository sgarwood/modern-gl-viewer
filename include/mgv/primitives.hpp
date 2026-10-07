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
///
/// Cut height is the single parameter that actually separates a green from
/// rough to the eye. Everything else follows from it: taller grass shades
/// itself, so it is darker and less glossy; it clumps, so it is patchier; and
/// it holds a bumpier surface. Tinting three otherwise identical surfaces
/// slightly different greens does not read at all.
struct SurfaceClass final {
    /// Height of cut, in metres. A putting surface is around 3 mm, a collar
    /// 10 mm, a fairway 13 mm, and rough 50 mm or more.
    float cut_height{};
    /// Which way the surface is mown, and how strongly, in [-1, 1].
    ///
    /// Negative is the green's pattern, positive the fairway's, and the two
    /// run across each other as they do on a real course. Zero is unmown, so
    /// a surface interpolating from green to fairway passes through no
    /// stripes at all, which is exactly what the collar between them is.
    float mow{};
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

/// Builds a single grass blade of unit height, standing on the origin.
///
/// The blade is a tapered strip narrowing to a point. It is deliberately
/// featureless: its height, width, lean, and colour all arrive as instance
/// parameters, and its curve is applied in the vertex shader, so one mesh
/// serves every blade on the course.
[[nodiscard]] MeshData make_grass_blade(int segments);

/// How a field of individual blades is scattered.
struct GrassFieldDescription final {
    /// Centre of the field, in the ground plane.
    Vec2 centre{};
    /// How far the field extends from its centre, in metres.
    float radius{9.0F};
    /// Blades per square metre on fully grown rough. Shorter cuts get
    /// proportionally fewer, because there is less of them to see.
    float density{320.0F};
    /// Blades shorter than this are not worth an instance: at a putting
    /// green's three millimetres they are below a pixel from any stance.
    float minimum_cut{0.008F};
    int segments{4};
    unsigned int seed{9'001u};
};

/// Scatters blades across the terrain, taking each one's height, width, and
/// lean from the height of cut where it stands.
///
/// This is near-field detail. Beyond a few metres a blade is smaller than a
/// pixel and the turf shader's filtered surface takes over, which is why the
/// field is bounded rather than covering the hole.
[[nodiscard]] MeshData make_grass_field(
    const GrassFieldDescription& field,
    const CourseTerrainDescription& terrain);

/// Builds a flat, subdivided square in the XZ plane, centred on the origin and
/// facing +Y. Subdivision exists so that per-vertex work and large triangles
/// do not interpolate badly across the ground.
[[nodiscard]] MeshData make_ground_plane(float size, int subdivisions);

} // namespace mgv
