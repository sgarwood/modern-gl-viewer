#include "mgv/course_session.hpp"

#include "mgv/obj_loader.hpp"

#include <cstddef>
#include <vector>
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

/// A prop instance, whose colour and roughness ride on the shared pipeline so
/// that every painted object is one material and many instances.
[[nodiscard]] std::shared_ptr<const MaterialInstance> painted(
    const std::shared_ptr<const Material>& material,
    Vec4 color_and_roughness) {
    MaterialInstance instance{material};
    instance.set_color("uBaseColorFactor", color_and_roughness);
    return std::make_shared<const MaterialInstance>(std::move(instance));
}

[[nodiscard]] Transform placed(Vec3 position, float yaw_radians = 0.0F, float scale = 1.0F) {
    Transform transform;
    transform.set_position(position)
        .set_rotation(Quaternion::from_axis_angle({0.0F, 1.0F, 0.0F}, yaw_radians))
        .set_uniform_scale(scale);
    return transform;
}

/// Where the trees stand. Clusters set back from the approach on both sides,
/// framing the hole without ever overhanging the line of play.
[[nodiscard]] std::vector<Vec3> tree_positions(const CourseTerrainDescription& terrain) {
    std::vector<Vec3> positions;
    unsigned int state{20'251'007u};
    const auto random = [&state](float low, float high) {
        state = state * 1664525u + 1013904223u;
        const auto unit =
            static_cast<float>((state >> 8u) & 0xFFFFFFu) / static_cast<float>(0x1000000u);
        return low + unit * (high - low);
    };

    for (int index = 0; index < 54; ++index) {
        const auto side = index % 2 == 0 ? -1.0F : 1.0F;
        // Positive `along` runs down the approach, matching the convention
        // course_surface_class uses, with a few trees set behind the green.
        const auto along = random(-terrain.green_half_extent * 3.4F, terrain.approach_length * 1.05F);
        const auto offset = terrain.approach_half_width + random(9.0F, 68.0F);
        const Vec3 position{side * offset, 0.0F, -along};
        // Nothing may stand on the putting surface.
        if (std::sqrt(position.x * position.x + position.z * position.z) <
            terrain.green_half_extent * 2.0F) {
            continue;
        }
        positions.push_back({
            position.x,
            course_terrain_height(position.x, position.z, terrain) - 0.15F,
            position.z,
        });
    }
    return positions;
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
    Renderable sky{
        std::make_shared<const MeshData>(make_sky_dome(2'000.0F, 24, 48)),
        instance_of(sky_material),
    };
    // The dome is drawn around the camera, so casting it would put the whole
    // world in shadow.
    sky.set_casts_shadow(false);
    scene.add(std::move(sky));
    scene.add(Renderable{
        std::make_shared<const MeshData>(make_course_terrain(description.terrain)),
        instance_of(turf_material),
    });
    auto prop_pipeline = load_material(shaders, "prop")->pipeline();
    prop_pipeline.rasterization.cull_mode = CullMode::none;
    const auto prop_material = std::make_shared<const Material>(std::move(prop_pipeline));

    auto foliage_pipeline = load_material(shaders, "foliage")->pipeline();
    foliage_pipeline.rasterization.cull_mode = CullMode::back;
    const auto foliage_material = std::make_shared<const Material>(std::move(foliage_pipeline));

    // --- the pin ------------------------------------------------------------
    // The hole sits a little off centre on the green, as a real pin position
    // does, and the stick is the tallest thing for twenty metres, so it is
    // what proves the shadow map is working.
    const Vec3 hole{1.9F, 0.0F, -2.4F};
    const auto hole_height = course_terrain_height(hole.x, hole.z, description.terrain);
    constexpr float pin_height = 2.13F;

    const auto cup = std::make_shared<const MeshData>(make_cylinder(0.054F, 0.054F, 0.11F, 16));
    scene.add(Renderable{
        cup,
        painted(prop_material, {0.015F, 0.013F, 0.011F, 0.95F}),
        placed({hole.x, hole_height - 0.105F, hole.z}),
    });

    const auto stick = std::make_shared<const MeshData>(
        make_cylinder(0.011F, 0.009F, pin_height, 10));
    scene.add(Renderable{
        stick,
        painted(prop_material, {0.62F, 0.62F, 0.60F, 0.42F}),
        placed({hole.x, hole_height, hole.z}),
    });

    const auto flag = std::make_shared<const MeshData>(make_flag(0.46F, 0.34F, 7));
    scene.add(Renderable{
        flag,
        painted(prop_material, {0.52F, 0.055F, 0.045F, 0.78F}),
        placed({hole.x, hole_height + pin_height - 0.40F, hole.z}, 2.1F),
    });

    // --- trees --------------------------------------------------------------
    // A handful of distinct meshes shared across many placements: enough
    // variety that no two neighbours match, without a mesh per tree.
    std::vector<std::shared_ptr<const MeshData>> tree_meshes;
    for (unsigned int variant = 0; variant < 5; ++variant) {
        tree_meshes.push_back(std::make_shared<const MeshData>(make_tree({
            .height = 7.5F + static_cast<float>(variant) * 1.6F,
            .trunk_radius = 0.22F + static_cast<float>(variant) * 0.035F,
            .canopy_lobes = 5 + static_cast<int>(variant % 3),
            .canopy_radius = 2.9F + static_cast<float>(variant) * 0.45F,
            .seed = 7u + variant * 131u,
        })));
    }
    std::size_t placement{};
    for (const auto& position : tree_positions(description.terrain)) {
        const auto& mesh = tree_meshes[placement % tree_meshes.size()];
        const auto yaw = static_cast<float>(placement) * 1.37F;
        const auto scale = 0.82F + static_cast<float>(placement % 7) * 0.07F;
        scene.add(Renderable{mesh, instance_of(foliage_material), placed(position, yaw, scale)});
        ++placement;
    }

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
    const Vec3 ball_start{
        description.ball_start.x,
        course_terrain_height(
            description.ball_start.x, description.ball_start.y, description.terrain) + ball_radius,
        description.ball_start.y,
    };
    static_cast<void>(engine.bind_golf_ball(
        ball,
        physics::RigidBodyBuilder{physics::Collider::sphere(physics::Length{ball_radius})}
            .at(physics::Position{ball_start})
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

    const auto on_terrain = [&description](Vec2 ground, float height) {
        return Vec3{
            ground.x,
            course_terrain_height(ground.x, ground.y, description.terrain) + height,
            ground.y,
        };
    };

    Camera camera;
    camera.look_at(
        on_terrain(description.viewpoint.position, description.viewpoint.eye_height),
        on_terrain(description.viewpoint.target, description.viewpoint.target_height));
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
    environment.fog_density = 0.0021F;
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
