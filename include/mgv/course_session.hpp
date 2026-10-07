#pragma once

#include "mgv/engine.hpp"
#include "mgv/lighting.hpp"
#include "mgv/renderer.hpp"

namespace mgv {

/// How a course session should be framed when it starts.
struct CourseViewpoint final {
    Vec3 position{0.0F, 1.7F, 12.0F};
    Vec3 target{0.0F, 0.6F, 0.0F};
    float vertical_field_of_view_degrees{55.0F};
    float near_plane{0.05F};
    float far_plane{2'000.0F};
};

/// Everything the engine needs to stand up a playable hole.
struct CourseSessionDescription final {
    AssetPaths assets;
    CourseViewpoint viewpoint;
    Environment environment;
};

/// Loads the course assets, binds the golf ball to a rigid body, installs the
/// static ground collider, and applies the viewpoint and environment.
///
/// Every frontend goes through this one function so that the interactive
/// window, the GLFW shell, and the offscreen capture tool all render the same
/// scene. Returns the entity that owns the ball.
EntityId configure_course_session(Engine& engine, const CourseSessionDescription& description);

/// The default description for a given asset set: a tee-box framing and a
/// clear late-morning sky.
[[nodiscard]] CourseSessionDescription default_course_session(AssetPaths assets);

} // namespace mgv
