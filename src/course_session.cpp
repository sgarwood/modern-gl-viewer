#include "mgv/course_session.hpp"

#include <stdexcept>
#include <utility>

namespace mgv {

EntityId configure_course_session(Engine& engine, const CourseSessionDescription& description) {
    const auto entities = engine.load(description.assets, ModelFit::authored);
    if (entities.empty()) {
        throw std::runtime_error{"A course session requires a renderable ball"};
    }

    const auto ball = entities.front();
    static_cast<void>(engine.bind_golf_ball(
        ball,
        physics::RigidBodyBuilder{physics::Collider::sphere(physics::Length{0.0213F})}
            .at(physics::Position{{0.0F, 0.0213F, 0.0F}})
            .mass(physics::Mass{0.04593F})
            .restitution(0.78F)
            .build()));

    // An unrendered slab under the turf catches anything the heightmap misses.
    static_cast<void>(engine.add_static_collider(
        physics::RigidBodyBuilder{
            physics::Collider::box(physics::Dimensions{{400.0F, 0.5F, 400.0F}})}
            .motion(physics::MotionType::static_body)
            .at(physics::Position{{0.0F, -0.25F, 0.0F}})
            .build()));

    Camera camera;
    camera.look_at(description.viewpoint.position, description.viewpoint.target);
    camera.set_perspective(
        description.viewpoint.vertical_field_of_view_degrees,
        description.viewpoint.near_plane,
        description.viewpoint.far_plane);
    engine.set_camera(std::move(camera));
    engine.set_environment(description.environment);

    return ball;
}

CourseSessionDescription default_course_session(AssetPaths assets) {
    Environment environment;
    environment.sun.direction = sun_direction_from_angles(140.0F, 50.0F);
    environment.exposure = 1.0F;
    return {.assets = std::move(assets), .viewpoint = {}, .environment = environment};
}

} // namespace mgv
