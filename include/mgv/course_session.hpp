#pragma once

#include "mgv/engine.hpp"
#include "mgv/lighting.hpp"
#include "mgv/primitives.hpp"
#include "mgv/renderer.hpp"

#include <filesystem>
#include <optional>

namespace mgv {

/// How a course session should be framed when it starts.
///
/// Positions are given in the ground plane with a height above the terrain,
/// not as absolute points. The hole falls about two metres over its length,
/// so an absolute eye height that works at the green puts the camera
/// underground on the fairway.
///
/// The default is an approach view from out on the fairway rather than a
/// stance on the green: standing on the putting surface fills the frame with
/// one cut of grass and hides the very distinctions the scene is built to
/// show.
struct CourseViewpoint final {
    /// Where the camera stands, in the ground plane.
    Vec2 position{5.2F, -47.0F};
    /// Eye height above the terrain, in metres. The default stands the camera
    /// on a rise behind the ball: a golfer's 1.6 m on a downhill lie
    /// foreshortens the green to nothing, which is true to life and useless
    /// for seeing the hole.
    float eye_height{6.4F};
    /// What the camera looks at, in the ground plane.
    Vec2 target{1.9F, -2.4F};
    /// Height above the terrain at the target, in metres.
    float target_height{0.5F};
    float vertical_field_of_view_degrees{42.0F};
    float near_plane{0.05F};
    float far_plane{4'000.0F};
};

/// The models and shader directory a hole is built from.
///
/// The terrain is generated rather than loaded: it has to agree exactly with
/// the analytic surface the physics heightmap uses, and it has to run out to
/// the horizon rather than stopping at the edge of a mesh.
struct CourseAssets final {
    std::filesystem::path ball_model;
    std::filesystem::path shader_directory;
};

/// The region of the course the ball can actually collide with.
///
/// The drawn terrain runs to the horizon, but a heightmap fine enough to putt
/// on cannot cover that much ground. The collider covers the corridor of
/// play; anything hit far outside it lands on a backstop well below.
struct CourseCollisionDescription final {
    Vec2 minimum{-70.0F, -230.0F};
    Vec2 maximum{70.0F, 40.0F};
    /// Spacing between samples, in metres. The green's undulation has a
    /// wavelength of about sixteen metres, so this resolves it to well under
    /// a millimetre after interpolation.
    float resolution{0.25F};
    /// Height of the backstop that catches anything leaving the heightmap.
    float backstop_height{-12.0F};
};

struct CourseSessionDescription final {
    CourseAssets assets;
    CourseTerrainDescription terrain;
    /// How the terrain is cut up for drawing. Its centre is ignored: the
    /// session keeps it on the camera.
    TerrainTessellation tessellation;
    CourseCollisionDescription collision;
    /// Near-field grass. Its centre defaults to the ball, which is where the
    /// player's attention is and where blades are large enough to see.
    GrassFieldDescription grass;
    /// Fallen leaves, drifted against the broadleaf trees.
    LeafLitterDescription litter;
    CourseViewpoint viewpoint;
    Environment environment;
    /// Where the ball is teed or lying, in the ground plane. Its height comes
    /// from the terrain, so it always starts resting on the surface.
    Vec2 ball_start{4.6F, -44.0F};
};

/// A hole that has been built, and the handles needed to keep it up to date
/// as the player moves through it.
struct CourseSession final {
    EntityId ball{};
    EntityId terrain{};
    /// Absent when the session was built with no blade field.
    std::optional<EntityId> grass{};
    /// Where the terrain's detail is currently centred, already snapped.
    Vec2 terrain_centre{};
    /// Where the blade field is currently centred.
    Vec2 grass_centre{};
};

/// Builds the sky dome, terrain, trees, leaves, grass, and ball, binds the
/// ball to a rigid body, installs the ground collider, and applies the
/// viewpoint and environment.
///
/// Every frontend goes through this one function so the interactive window and
/// the offscreen capture tool render the same scene.
[[nodiscard]] CourseSession configure_course_session(
    Engine& engine,
    const CourseSessionDescription& description);

/// Moves the terrain's detail and the blade field to follow the camera,
/// rebuilding their geometry when it has walked far enough to matter.
///
/// Both are built around a centre. Left where they were created, the fine
/// tessellation and every blade of grass stay behind as the player walks up
/// the hole, which is the whole reason the hike could not be played.
///
/// Returns true if anything was rebuilt this call. Cheap when nothing has:
/// the terrain only moves on whole steps of its own innermost grid, so a
/// camera drifting within one cell costs a comparison.
bool stream_course(Engine& engine, CourseSession& session, const CourseSessionDescription& description);

/// A clear late-morning round on the bundled assets.
[[nodiscard]] CourseSessionDescription default_course_session(
    const std::filesystem::path& asset_directory);

/// The linear exposure multiplier for a given photographic exposure value at
/// ISO 100, following the standard saturation-based relation. Daylight sits
/// near EV 15.
[[nodiscard]] float exposure_from_ev100(float ev100) noexcept;

} // namespace mgv
