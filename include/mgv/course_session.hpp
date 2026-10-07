#pragma once

#include "mgv/engine.hpp"
#include "mgv/lighting.hpp"
#include "mgv/primitives.hpp"
#include "mgv/renderer.hpp"

#include <filesystem>

namespace mgv {

/// How a course session should be framed when it starts.
struct CourseViewpoint final {
    Vec3 position{3.1F, 2.35F, 8.6F};
    Vec3 target{-0.4F, 0.05F, -9.0F};
    float vertical_field_of_view_degrees{46.0F};
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

struct CourseSessionDescription final {
    CourseAssets assets;
    CourseTerrainDescription terrain;
    CourseViewpoint viewpoint;
    Environment environment;
};

/// Builds the sky dome, terrain, and ball, binds the ball to a rigid body,
/// installs the ground collider, and applies the viewpoint and environment.
///
/// Every frontend goes through this one function so the interactive window and
/// the offscreen capture tool render the same scene. Returns the ball entity.
EntityId configure_course_session(Engine& engine, const CourseSessionDescription& description);

/// A clear late-morning round on the bundled assets.
[[nodiscard]] CourseSessionDescription default_course_session(
    const std::filesystem::path& asset_directory);

/// The linear exposure multiplier for a given photographic exposure value at
/// ISO 100, following the standard saturation-based relation. Daylight sits
/// near EV 15.
[[nodiscard]] float exposure_from_ev100(float ev100) noexcept;

} // namespace mgv
