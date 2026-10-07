#include "mgv/course_session.hpp"

#include "mgv/obj_loader.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <span>
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

/// One tree standing on the course.
struct TreePlacement final {
    TreeSpecies species{};
    /// Which of the species' generated meshes this one uses.
    std::size_t variant{};
    Vec3 position{};
    float yaw{};
    float scale{1.0F};
};

/// Where the trees stand, and what they are.
///
/// Clusters set back from the approach on both sides, framing the hole
/// without ever overhanging the line of play. Species come in runs rather
/// than shuffled, because trees of a kind grow together and a perfectly
/// mixed wood reads as scattered props.
[[nodiscard]] std::vector<TreePlacement> tree_placements(
    const CourseTerrainDescription& terrain,
    std::size_t variants_per_species) {
    constexpr std::array species{
        TreeSpecies::oak,
        TreeSpecies::beech,
        TreeSpecies::maple,
        TreeSpecies::pine,
    };

    std::vector<TreePlacement> placements;
    unsigned int state{20'251'007u};
    const auto random = [&state](float low, float high) {
        state = state * 1664525u + 1013904223u;
        const auto unit =
            static_cast<float>((state >> 8u) & 0xFFFFFFu) / static_cast<float>(0x1000000u);
        return low + unit * (high - low);
    };

    // Trees of a kind grow together, so the hole is planted in stands rather
    // than in a shuffle. Each stand takes a centre off one side of the
    // approach and scatters its own species around it; a perfectly mixed
    // wood reads as scattered props, and alternating sides tree by tree --
    // which an index-driven run would do -- is not a stand at all.
    for (int stand = 0; stand < 16; ++stand) {
        const auto chosen = species[static_cast<std::size_t>(
            std::min(random(0.0F, static_cast<float>(species.size())),
                     static_cast<float>(species.size()) - 0.001F))];
        const auto side = random(0.0F, 1.0F) < 0.5F ? -1.0F : 1.0F;
        const auto along = random(-terrain.green_half_extent * 3.0F, terrain.approach_length);
        const auto offset = terrain.approach_half_width + random(11.0F, 62.0F);
        const Vec2 centre{side * offset, -along};
        const auto trees_in_stand = static_cast<int>(random(4.0F, 10.0F));

        for (int index = 0; index < trees_in_stand; ++index) {
            const auto scatter = random(0.0F, 2.0F * 3.14159265F);
            const auto distance = random(0.0F, 1.0F);
            const auto x = centre.x + std::cos(scatter) * distance * 19.0F;
            const auto z = centre.y + std::sin(scatter) * distance * 19.0F;

            // Nothing may stand on the putting surface, nor out in the line
            // of play.
            if (std::sqrt(x * x + z * z) < terrain.green_half_extent * 2.0F ||
                std::abs(x) < terrain.approach_half_width + 4.0F) {
                continue;
            }

            placements.push_back({
                .species = chosen,
                .variant = static_cast<std::size_t>(
                               random(0.0F, static_cast<float>(variants_per_species))) %
                           std::max<std::size_t>(variants_per_species, 1),
                // Set a little into the ground, so no tree appears to balance
                // on the surface where the terrain dips beneath it.
                .position = {x, course_terrain_height(x, z, terrain) - 0.18F, z},
                .yaw = random(0.0F, 6.2831853F),
                // Pines run taller and narrower than the broadleaves beside
                // them.
                .scale = chosen == TreeSpecies::pine ? random(0.92F, 1.34F)
                                                     : random(0.78F, 1.14F),
            });
        }
    }
    return placements;
}

/// Where leaves have drifted.
///
/// Leaves collect under and just downwind of the trees that shed them, so
/// the drifts are placed against the trees rather than scattered over the
/// hole. Pines are skipped: they drop needles, not leaf litter.
[[nodiscard]] std::vector<LeafPile> leaf_piles(
    std::span<const TreePlacement> trees,
    float wind_direction_radians) {
    std::vector<LeafPile> piles;
    unsigned int state{777'301u};
    const auto random = [&state](float low, float high) {
        state = state * 1664525u + 1013904223u;
        const auto unit =
            static_cast<float>((state >> 8u) & 0xFFFFFFu) / static_cast<float>(0x1000000u);
        return low + unit * (high - low);
    };

    const Vec2 downwind{std::sin(wind_direction_radians), -std::cos(wind_direction_radians)};
    for (const auto& tree : trees) {
        if (tree.species == TreeSpecies::pine || random(0.0F, 1.0F) > 0.55F) {
            continue;
        }
        // Just off the trunk, carried a little downwind, as a drift does.
        const auto drift = random(0.6F, 3.4F);
        const auto scatter = random(-1.8F, 1.8F);
        piles.push_back({
            .centre = {
                tree.position.x + downwind.x * drift + scatter,
                tree.position.z + downwind.y * drift - scatter * 0.6F,
            },
            .radius = random(0.9F, 2.3F) * tree.scale,
            .depth = random(0.05F, 0.16F),
        });
    }
    return piles;
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
    // Each species is drawn once, with every tree of that kind as an
    // instance, so ninety-odd trees cost four draws rather than ninety.
    // Colour rides on the material instance, since it is the same for every
    // tree of a species and a uniform is cheaper than a vertex attribute
    // repeated across a hundred thousand vertices.
    constexpr std::size_t variants_per_species = 3;
    const auto placements = tree_placements(description.terrain, variants_per_species);

    for (const auto species : {TreeSpecies::oak,
                               TreeSpecies::beech,
                               TreeSpecies::maple,
                               TreeSpecies::pine}) {
        for (std::size_t variant = 0; variant < variants_per_species; ++variant) {
            std::vector<MeshInstance> instances;
            for (const auto& tree : placements) {
                if (tree.species != species || tree.variant != variant) {
                    continue;
                }
                instances.push_back({
                    .position = tree.position,
                    .yaw = tree.yaw,
                    .parameters = {tree.scale, 0.0F, 0.0F, 0.0F},
                });
            }
            if (instances.empty()) {
                continue;
            }

            auto mesh = make_tree({
                .species = species,
                .height = 9.5F + static_cast<float>(variant) * 2.1F,
                .spread = 0.88F + static_cast<float>(variant) * 0.14F,
                .seed = 17u + static_cast<unsigned int>(variant) * 311u +
                        static_cast<unsigned int>(species) * 7919u,
            });
            mesh.instances = std::move(instances);

            const auto palette = tree_palette(species);
            MaterialInstance material{foliage_material};
            material.set_color("uBarkColor", {palette.bark.x, palette.bark.y, palette.bark.z, 1.0F});
            material.set_color(
                "uLeafShadeColor",
                {palette.leaf_shade.x, palette.leaf_shade.y, palette.leaf_shade.z, 1.0F});
            material.set_color(
                "uLeafSunColor",
                {palette.leaf_sun.x, palette.leaf_sun.y, palette.leaf_sun.z, 1.0F});
            // Needles scatter light differently from a broad leaf, and a
            // conifer's crown is far denser.
            material.set_color(
                "uFoliageResponse",
                {species == TreeSpecies::pine ? 0.45F : 1.0F,
                 species == TreeSpecies::pine ? 0.22F : 1.0F,
                 0.0F,
                 0.0F});

            scene.add(Renderable{
                std::make_shared<const MeshData>(std::move(mesh)),
                std::make_shared<const MaterialInstance>(std::move(material)),
            });
        }
    }

    // --- fallen leaves --------------------------------------------------------
    auto leaf_pipeline = load_material(shaders, "leaf")->pipeline();
    leaf_pipeline.rasterization.cull_mode = CullMode::none;
    const auto leaf_material = std::make_shared<const Material>(std::move(leaf_pipeline));

    const auto piles = leaf_piles(placements, description.environment.wind_direction_radians);
    if (!piles.empty() && description.litter.density > 0.0F) {
        Renderable litter{
            std::make_shared<const MeshData>(
                make_leaf_litter(piles, description.litter, description.terrain)),
            instance_of(leaf_material),
        };
        // Tens of thousands of leaves, each finer than a shadow-map texel.
        litter.set_casts_shadow(false);
        scene.add(std::move(litter));
    }

    // --- near-field grass ---------------------------------------------------
    // Individual blades are only worth drawing within a few metres; beyond
    // that the turf shader's filtered surface is both cheaper and steadier.
    auto grass_pipeline = load_material(shaders, "grass")->pipeline();
    grass_pipeline.rasterization.cull_mode = CullMode::none;
    const auto grass_material = std::make_shared<const Material>(std::move(grass_pipeline));

    auto grass_field = description.grass;
    if (grass_field.radius > 0.0F && grass_field.density > 0.0F) {
        if (grass_field.centre == Vec2{}) {
            grass_field.centre = description.ball_start;
        }
        Renderable grass{
            std::make_shared<const MeshData>(make_grass_field(grass_field, description.terrain)),
            instance_of(grass_material),
        };
        // Shadow-mapping a hundred thousand blades across the whole volume
        // would spend the entire map on detail finer than one of its texels.
        grass.set_casts_shadow(false);
        scene.add(std::move(grass));
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

    // The ball has to roll on the same surface that is drawn, so the collider
    // is sampled from the very function the terrain mesh is built from.
    const auto& collision = description.collision;
    const auto spacing = std::max(collision.resolution, 0.01F);
    const auto columns = static_cast<int>(
        std::ceil((collision.maximum.x - collision.minimum.x) / spacing)) + 1;
    const auto rows = static_cast<int>(
        std::ceil((collision.maximum.y - collision.minimum.y) / spacing)) + 1;
    if (columns < 2 || rows < 2) {
        throw std::invalid_argument{"Course collision region is smaller than one sample"};
    }

    std::vector<float> heights;
    heights.reserve(static_cast<std::size_t>(columns) * static_cast<std::size_t>(rows));
    for (int row = 0; row < rows; ++row) {
        const auto z = collision.minimum.y + static_cast<float>(row) * spacing;
        for (int column = 0; column < columns; ++column) {
            const auto x = collision.minimum.x + static_cast<float>(column) * spacing;
            heights.push_back(course_terrain_height(x, z, description.terrain));
        }
    }

    static_cast<void>(engine.add_static_collider(
        physics::RigidBodyBuilder{
            physics::Collider::heightmap(columns, rows, spacing, spacing, std::move(heights))}
            .motion(physics::MotionType::static_body)
            .at(physics::Position{{collision.minimum.x, 0.0F, collision.minimum.y}})
            .build()));

    // A backstop far below, so a ball driven off the heightmap comes to rest
    // instead of falling forever.
    static_cast<void>(engine.add_static_collider(
        physics::RigidBodyBuilder{
            physics::Collider::box(physics::Dimensions{{2'000.0F, 0.5F, 2'000.0F}})}
            .motion(physics::MotionType::static_body)
            .at(physics::Position{{0.0F, collision.backstop_height - 0.5F, 0.0F}})
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
        .collision = {},
        .grass = {},
        .litter = {},
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
