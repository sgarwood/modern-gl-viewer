#include "mgv/course_session.hpp"

#include "mgv/obj_loader.hpp"
#include "mgv/primitives.hpp"
#include "mgv/shader_loader.hpp"

#include <cmath>
#include <memory>
#include <stdexcept>
#include <utility>

namespace mgv {
namespace {

/// Loads a vertex/fragment pair by stem from the session's shader directory.
[[nodiscard]] std::shared_ptr<const Material> load_material(
    const std::filesystem::path& shader_directory,
    const std::string& stem) {
    return std::make_shared<const Material>(ShaderLoader::load(
        shader_directory / (stem + ".vert"),
        shader_directory / (stem + ".frag")));
}

[[nodiscard]] std::shared_ptr<const MaterialInstance> instance_of(
    std::shared_ptr<const Material> material) {
    return std::make_shared<const MaterialInstance>(MaterialInstance{std::move(material)});
}

/// Merges every primitive of an imported model into one mesh.
///
/// The course meshes are single-material surfaces, and collapsing them keeps
/// one draw call per surface rather than one per OBJ group.
[[nodiscard]] MeshData flatten(const ImportedModel& model) {
    MeshData merged;
    for (const auto& primitive : model.primitives) {
        const auto offset = static_cast<std::uint32_t>(merged.vertices.size());
        merged.vertices.insert(
            merged.vertices.end(), primitive.mesh.vertices.begin(), primitive.mesh.vertices.end());
        for (const auto index : primitive.mesh.indices) {
            merged.indices.push_back(offset + index);
        }
    }
    if (merged.empty()) {
        throw std::runtime_error{"Course model contains no geometry"};
    }
    return merged;
}

} // namespace

EntityId configure_course_session(Engine& engine, const CourseSessionDescription& description) {
    const auto& shaders = description.assets.shader_directory;

    // The dome is drawn around the eye by its vertex shader, so its depth
    // state is the only thing that has to be special: no writes, and a test
    // that lets it fill whatever the scene did not cover.
    auto sky_pipeline = load_material(shaders, "sky")->pipeline();
    sky_pipeline.depth.write_enabled = false;
    sky_pipeline.depth.compare = CompareOperation::less_or_equal;
    sky_pipeline.rasterization.cull_mode = CullMode::none;
    const auto sky_material = std::make_shared<const Material>(std::move(sky_pipeline));

    auto turf_pipeline = load_material(shaders, "turf")->pipeline();
    turf_pipeline.rasterization.cull_mode = CullMode::back;
    const auto turf_material = std::make_shared<const Material>(std::move(turf_pipeline));

    auto ball_pipeline = load_material(shaders, "ball")->pipeline();
    ball_pipeline.rasterization.cull_mode = CullMode::back;
    const auto ball_material = std::make_shared<const Material>(std::move(ball_pipeline));

    Scene scene;
    scene.add(Renderable{
        std::make_shared<const MeshData>(make_sky_dome(2'000.0F, 24, 48)),
        instance_of(sky_material),
    });
    scene.add(Renderable{
        std::make_shared<const MeshData>(make_course_terrain(description.terrain)),
        instance_of(turf_material),
    });
    const auto ball_index = scene.size();
    scene.add(Renderable{
        std::make_shared<const MeshData>(
            flatten(ObjLoader{}.load_model(description.assets.ball_model))),
        instance_of(ball_material),
    });

    const auto entities = engine.set_scene(std::move(scene));
    if (entities.size() <= ball_index) {
        throw std::runtime_error{"A course session requires a renderable ball"};
    }
    const auto ball = entities[ball_index];

    constexpr float ball_radius = 0.021335F;
    static_cast<void>(engine.bind_golf_ball(
        ball,
        physics::RigidBodyBuilder{physics::Collider::sphere(physics::Length{ball_radius})}
            .at(physics::Position{{0.0F, ball_radius, 0.0F}})
            .mass(physics::Mass{0.04593F})
            .restitution(0.78F)
            .build()));

    // An unrendered slab under the turf catches anything the terrain misses.
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

CourseSessionDescription default_course_session(const std::filesystem::path& asset_directory) {
    Environment environment;
    environment.sun.direction = sun_direction_from_angles(158.0F, 43.0F);
    environment.sun.color = {1.0F, 0.96F, 0.90F};
    environment.sun.illuminance = 98'000.0F;
    environment.sky_illuminance = 21'000.0F;
    environment.sky_zenith_color = {0.21F, 0.38F, 0.72F};
    environment.sky_horizon_color = {0.70F, 0.78F, 0.86F};
    environment.ground_albedo = {0.085F, 0.125F, 0.055F};
    environment.turbidity = 2.8F;
    environment.fog_density = 0.0035F;
    environment.fog_height_falloff = 0.09F;
    environment.surface_wetness = 0.25F;
    environment.exposure = exposure_from_ev100(14.0F);

    return {
        .assets = {
            .ball_model = asset_directory / "ball.obj",
            .shader_directory = asset_directory / "shaders",
        },
        .terrain = {},
        .viewpoint = {},
        .environment = environment,
    };
}

float exposure_from_ev100(float ev100) noexcept {
    // The saturation-based speed relation: the luminance that saturates the
    // sensor is 1.2 * 2^EV100, and exposure is its reciprocal.
    return 1.0F / (1.2F * std::pow(2.0F, ev100));
}

} // namespace mgv
