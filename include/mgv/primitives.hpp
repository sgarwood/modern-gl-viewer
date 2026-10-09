#pragma once

#include "mgv/mesh.hpp"

#include <span>

namespace mgv {

/// Builds an inward-facing hemisphere-plus-skirt used as a sky dome.
///
/// The skirt continues a little below the horizon so a camera tilted down at
/// the ball still has sky geometry behind distant terrain. Normals point
/// inwards, towards the eye at the centre.
[[nodiscard]] MeshData make_sky_dome(float radius, int rings, int sectors);

/// Where the course's features are. This is the hole itself, fixed in the
/// world, and it is what `course_terrain_height` and `course_surface_class`
/// are expressed in terms of.
struct CourseTerrainDescription final {
    /// Half the width of the mown green, in metres.
    float green_half_extent{7.62F};
    /// Half-width of the mown approach running away from the green.
    float approach_half_width{17.0F};
    /// How far the approach runs before it gives way to rough, in metres.
    float approach_length{190.0F};
};

/// How finely the terrain is cut up for drawing, and where that detail sits.
///
/// Separate from the course itself, because the two are anchored to
/// different things: the hole stays where it is, while the detail has to
/// follow the player. Nested square loops coarsening outwards is the
/// structure of a geometry clipmap, and consecutive loops here carry the
/// same vertex count, so it is watertight with no T-junctions to stitch.
struct TerrainTessellation final {
    /// Where the detail is centred, in the ground plane. Snapped internally.
    Vec2 centre{};
    /// Half the width of the finely tessellated square at the centre.
    float inner_half_extent{9.0F};
    /// Quads per side across that square. A green undulates by around a
    /// tenth of a metre, so this has to stay fine enough to putt on.
    int inner_resolution{96};
    /// Half the width of the whole terrain, in metres.
    float outer_extent{420.0F};
    /// Concentric loops between the centre and the outer edge. Each is a
    /// fixed fraction wider than the last, so triangle density falls off
    /// with distance while staying fine enough near the player that the
    /// ground does not visibly facet.
    int rings{52};
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

/// The spacing of the innermost grid, in metres.
[[nodiscard]] float terrain_inner_step(const TerrainTessellation& tessellation) noexcept;

/// The centre a tessellation will actually be built at.
///
/// Snapped to whole steps of the innermost grid. Without that the sampling
/// grid slides continuously under the terrain as the viewer walks, and every
/// feature finer than a grid cell crawls -- the same reason a shadow map's
/// volume is snapped to its texels.
[[nodiscard]] Vec2 snapped_terrain_centre(const TerrainTessellation& tessellation) noexcept;

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

/// Builds the terrain as a single watertight mesh, with its detail centred
/// where the tessellation asks.
[[nodiscard]] MeshData make_course_terrain(
    const CourseTerrainDescription& description,
    const TerrainTessellation& tessellation);

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

/// The species a tree is grown as.
///
/// Silhouette is what identifies a tree at a hundred metres, long before
/// colour does, so each species differs first in how its crown is built: a
/// pine in whorls around a bare lower trunk, an oak in wide low masses, a
/// beech in a tall egg held high, a maple in a tight round head.
enum class TreeSpecies {
    oak,
    beech,
    maple,
    pine,
};

/// A tree, as one mesh.
///
/// `tex_coord.x` marks material: 0 for bark, 1 for canopy, so a single
/// shader lights both without a second draw call per tree.
struct TreeDescription final {
    TreeSpecies species{TreeSpecies::oak};
    /// Overall height, in metres.
    float height{11.0F};
    /// Multiplies the species' natural crown width.
    float spread{1.0F};
    /// Seed for the per-tree variation in branch and lobe placement.
    unsigned int seed{1u};
};

/// The colours a species is lit with, in linear sRGB.
///
/// They are kept out of the mesh because every tree of a species shares them,
/// and a shader uniform is cheaper than a vertex attribute repeated across a
/// hundred thousand vertices.
struct TreePalette final {
    Vec3 bark;
    /// Foliage deep inside the crown, in its own shade.
    Vec3 leaf_shade;
    /// Foliage on the outside, which is younger and catches the sun.
    Vec3 leaf_sun;
};

[[nodiscard]] TreePalette tree_palette(TreeSpecies species) noexcept;

/// The bole of a tree, as the physics needs it.
///
/// Taken from the same species proportions the mesh is built from, so the
/// collider and the bark a player can see are the same object. Derived
/// separately, they would drift, and a ball would pass through one tree and
/// bounce off thin air beside the next.
struct TreeTrunk final {
    /// Radius of the bole, in metres.
    float radius{};
    /// How far up the bole is solid, in metres above the tree's base. Above
    /// it is crown: a broadleaf divides into limbs the ball flies between,
    /// and nothing here pretends to catch it.
    float height{};
};

[[nodiscard]] TreeTrunk tree_trunk(const TreeDescription& description) noexcept;

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

/// Builds a single fallen leaf, lying in the XZ plane and pointing +Z, of
/// unit length. Its size, tilt, colour, and depth in the pile all arrive as
/// instance parameters.
[[nodiscard]] MeshData make_leaf(int segments);

/// A drift of fallen leaves.
struct LeafPile final {
    /// Centre of the drift, in the ground plane.
    Vec2 centre{};
    /// How far it spreads, in metres.
    float radius{1.3F};
    /// How deep it is heaped at the centre, in metres.
    float depth{0.11F};
};

struct LeafLitterDescription final {
    /// Leaves per square metre at the heart of a drift.
    float density{2'600.0F};
    /// Length of a leaf, in metres.
    float leaf_length{0.085F};
    int segments{5};
    unsigned int seed{4'071u};
};

/// Scatters leaves through the given drifts, heaped into a mound and resting
/// on the terrain.
///
/// Leaves fill the volume of the mound rather than tiling its surface, so a
/// ball dropped into one is genuinely among them rather than sitting on a
/// painted disc. Each leaf carries how deep it lies, which is what lets the
/// shader darken the ones underneath.
[[nodiscard]] MeshData make_leaf_litter(
    std::span<const LeafPile> piles,
    const LeafLitterDescription& litter,
    const CourseTerrainDescription& terrain);

/// Builds a flat, subdivided square in the XZ plane, centred on the origin and
/// facing +Y. Subdivision exists so that per-vertex work and large triangles
/// do not interpolate badly across the ground.
[[nodiscard]] MeshData make_ground_plane(float size, int subdivisions);

} // namespace mgv
