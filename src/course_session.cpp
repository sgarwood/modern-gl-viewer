#include "mgv/course_session.hpp"

#include "mgv/gltf_loader.hpp"
#include "mgv/obj_loader.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <optional>
#include <span>
#include <vector>
#include "mgv/primitives.hpp"
#include "mgv/stl_loader.hpp"
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

/// The snapped bottom-left corner of the collidable patch, which is where a
/// heightmap collider's body sits.
[[nodiscard]] Vec2 snapped_collision_centre(
    const CourseCollisionDescription& collision,
    Vec2 around) {
    // Snapped for the same reason the terrain's grid is: an unsnapped patch
    // resamples the ground at a slightly different offset every rebuild, and
    // a putt crossing a rebuild would feel the surface step under it.
    const auto step = std::max(collision.resolution, 1.0e-3F) * 4.0F;
    return {std::floor(around.x / step) * step, std::floor(around.y / step) * step};
}

[[nodiscard]] physics::Position collision_origin(
    const CourseCollisionDescription& collision,
    Vec2 around) {
    const auto centre = snapped_collision_centre(collision, around);
    return physics::Position{
        {centre.x - collision.half_extent, 0.0F, centre.y - collision.half_extent}};
}

/// Samples the terrain into a heightmap collider centred on `around`.
[[nodiscard]] physics::Collider collision_patch(
    const CourseCollisionDescription& collision,
    const CourseTerrainDescription& terrain,
    Vec2 around) {
    const auto spacing = std::max(collision.resolution, 0.01F);
    const auto samples =
        static_cast<int>(std::ceil(2.0F * collision.half_extent / spacing)) + 1;
    if (samples < 2) {
        throw std::invalid_argument{"Course collision patch is smaller than one sample"};
    }
    const auto origin = collision_origin(collision, around);
    const auto corner = origin.metres();

    std::vector<float> heights;
    heights.reserve(static_cast<std::size_t>(samples) * static_cast<std::size_t>(samples));
    for (int row = 0; row < samples; ++row) {
        const auto z = corner.z + static_cast<float>(row) * spacing;
        for (int column = 0; column < samples; ++column) {
            const auto x = corner.x + static_cast<float>(column) * spacing;
            heights.push_back(course_terrain_height(x, z, terrain));
        }
    }
    return physics::Collider::heightmap(samples, samples, spacing, spacing, std::move(heights));
}

/// A single white pixel, for shaders that sample a base colour texture on a
/// model that did not bring one.
[[nodiscard]] std::shared_ptr<const Texture> white_texture() {
    static const auto texture = std::make_shared<const Texture>(ImageData{
        1, 1, PixelFormat::rgba8_unorm, ColorSpace::srgb, {255, 255, 255, 255}});
    return texture;
}

/// The generated course, presented to the round as ground it can walk on.
class CourseGround final : public game::GroundHeights {
public:
    explicit CourseGround(CourseTerrainDescription terrain) : terrain_{terrain} {}

    [[nodiscard]] float height_at(float x, float z) const override {
        return course_terrain_height(x, z, terrain_);
    }

private:
    CourseTerrainDescription terrain_;
};

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

/// Owns the round and the ground it refers to, so a session can be passed
/// around without the reference dangling.
class CourseRound final {
public:
    CourseRound(CourseTerrainDescription terrain, game::RoundRules rules)
        : ground{terrain}, round{rules, ground} {}

    CourseGround ground;
    game::Round round;
};

CourseSession configure_course_session(Engine& engine, const CourseSessionDescription& description) {
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
    auto tessellation = description.tessellation;
    tessellation.centre = description.viewpoint.position;
    const auto terrain_index = scene.size();
    scene.add(Renderable{
        std::make_shared<const MeshData>(make_course_terrain(description.terrain, tessellation)),
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
    const Vec3 hole{description.hole.x, 0.0F, description.hole.y};
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
    std::optional<std::size_t> grass_index;
    if (grass_field.radius > 0.0F && grass_field.density > 0.0F) {
        if (grass_field.centre == Vec2{}) {
            grass_field.centre = description.ball_start;
        }
        grass_index = scene.size();
        Renderable grass{
            std::make_shared<const MeshData>(make_grass_field(grass_field, description.terrain)),
            instance_of(grass_material),
        };
        // Shadow-mapping a hundred thousand blades across the whole volume
        // would spend the entire map on detail finer than one of its texels.
        grass.set_casts_shadow(false);
        scene.add(std::move(grass));
    }

    // --- a character ---------------------------------------------------------
    std::optional<std::size_t> character_index;
    std::optional<std::size_t> club_index;
    std::optional<ImportedSkin> character_skin;
    if (description.character) {
        const auto& wanted = *description.character;
        auto imported = GltfLoader{}.load(wanted.model);
        auto character_material = load_material(shaders, "skinned");
        auto skinned_pipeline = character_material->pipeline();
        skinned_pipeline.rasterization.cull_mode = CullMode::none;
        const auto skinned_material =
            std::make_shared<const Material>(std::move(skinned_pipeline));

        auto mesh = flatten(imported);
        // flatten drops the skinning stream, so carry it across from the one
        // primitive that has it. A character is a single skinned mesh in
        // every export worth loading.
        for (const auto& primitive : imported.primitives) {
            if (primitive.mesh.skinned()) {
                mesh.skinning = primitive.mesh.skinning;
                break;
            }
        }

        const auto ground = course_terrain_height(
            wanted.position.x, wanted.position.y, description.terrain);
        constexpr float degrees_to_radians = 3.14159265F / 180.0F;
        const auto facing = Quaternion::from_axis_angle(
            {0.0F, 1.0F, 0.0F},
            (wanted.facing_degrees + wanted.facing_offset_degrees) * degrees_to_radians);
        // The import rotation goes on the model matrix rather than into the
        // vertices, because the skinning palette is in the skeleton's own
        // space: rotating the mesh without rotating the palette would just
        // tear the character apart.
        const auto upright = wanted.z_up
            ? Quaternion::from_axis_angle({1.0F, 0.0F, 0.0F}, -90.0F * degrees_to_radians)
            : Quaternion{};
        Transform placement;
        placement.set_position({wanted.position.x, ground, wanted.position.y})
            .set_rotation(facing * upright)
            .set_uniform_scale(wanted.scale);

        MaterialInstance instance{skinned_material};
        const auto albedo = imported.materials.empty()
            ? Vec3{0.62F, 0.60F, 0.58F}
            : imported.materials.front().diffuse_color;
        instance.set_color("uBaseColorFactor", {albedo.x, albedo.y, albedo.z, 0.65F});
        // A shader that samples a texture needs one bound whether the model
        // brought one or not; an unbound sampler reads as black, which turns
        // an untextured character into a silhouette.
        instance.set_texture(
            "uBaseColorTexture",
            !imported.materials.empty() && imported.materials.front().diffuse_image
                ? std::make_shared<const Texture>(
                      *imported.materials.front().diffuse_image,
                      imported.materials.front().sampler)
                : white_texture());

        if (description.character->club) {
            const auto& club = *description.character->club;
            // Steel: dark enough to read against the turf, smooth enough to
            // catch the sun along the shaft.
            club_index = scene.size();
            scene.add(Renderable{
                std::make_shared<const MeshData>(load_binary_stl(club.model)),
                painted(prop_material, {0.42F, 0.44F, 0.47F, 0.22F}),
                Transform{},
            });
        }

        character_index = scene.size();
        character_skin = imported.skin;
        scene.add(Renderable{
            std::make_shared<const MeshData>(std::move(mesh)),
            std::make_shared<const MaterialInstance>(std::move(instance)),
            placement,
        });
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

    // The ball has to roll on the same surface that is drawn, so the
    // collider is sampled from the very function the terrain mesh is built
    // from. It follows the ball; see stream_course.
    const auto ground = engine.add_static_collider(
        physics::RigidBodyBuilder{
            collision_patch(description.collision, description.terrain, description.ball_start)}
            .motion(physics::MotionType::static_body)
            .at(collision_origin(description.collision, description.ball_start))
            .build());

    // A backstop far below, so a ball driven clear of the patch comes to
    // rest instead of falling forever.
    static_cast<void>(engine.add_static_collider(
        physics::RigidBodyBuilder{
            physics::Collider::box(physics::Dimensions{{4'000.0F, 0.5F, 4'000.0F}})}
            .motion(physics::MotionType::static_body)
            .at(physics::Position{{0.0F, description.collision.backstop_height - 0.5F, 0.0F}})
            .build()));

    const auto on_terrain = [&description](Vec2 place, float height) {
        return Vec3{
            place.x,
            course_terrain_height(place.x, place.y, description.terrain) + height,
            place.y,
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

    std::optional<EntityId> character_entity;
    std::optional<animation::AnimationPlayerId> character_player;
    std::optional<animation::AnimationClipId> idle_clip;
    std::optional<animation::AnimationClipId> follow_through_clip;
    if (character_index && character_skin && description.character) {
        const auto entity = entities[*character_index];
        idle_clip = engine.load_animation({
            .skeleton = description.character->skeleton,
            .animation = description.character->animation,
        });
        if (!description.character->follow_through.empty()) {
            follow_through_clip = engine.load_animation({
                .skeleton = description.character->skeleton,
                .animation = description.character->follow_through,
            });
        }
        const auto player = engine.bind_animation(entity, *idle_clip);
        engine.bind_skin(entity, player, *character_skin);
        engine.enqueue(PlayAnimationCommand{player});
        character_player = player;
        character_entity = entity;
    }

    auto rules = description.round;
    rules.hole = description.hole;

    return {
        .ball = ball,
        .terrain = entities[terrain_index],
        .ground = ground,
        .collision_centre = snapped_collision_centre(description.collision, description.ball_start),
        .grass = grass_index ? std::optional<EntityId>{entities[*grass_index]} : std::nullopt,
        .terrain_centre = snapped_terrain_centre(tessellation),
        .grass_centre = grass_field.centre,
        .character = character_entity,
        .character_z_up = description.character && description.character->z_up,
        .character_facing_offset_degrees = description.character
            ? description.character->facing_offset_degrees
            : 0.0F,
        .character_scale = description.character ? description.character->scale : 1.0F,
        .club_entity = club_index ? std::optional<EntityId>{entities[*club_index]}
                                  : std::nullopt,
        .club = description.character ? description.character->club : std::nullopt,
        .character_player = character_player,
        .idle_clip = idle_clip,
        .follow_through_clip = follow_through_clip,
        .round = std::make_shared<CourseRound>(description.terrain, rules),
    };
}

game::RoundState round_state(const CourseSession& session) {
    return session.round ? session.round->round.state() : game::RoundState::addressing;
}

game::RoundEvent advance_round(
    Engine& engine,
    const CourseSession& session,
    const CourseSessionDescription& description) {
    if (!session.round) {
        return game::RoundEvent::none;
    }
    const auto lie = engine.transform(session.ball).position();
    const auto velocity = engine.linear_velocity(session.ball);

    const auto update = session.round->round.update({
        .elapsed_seconds = engine.last_tick_seconds(),
        .ball_position = lie,
        .ball_velocity = velocity ? velocity->metres_per_second() : Vec3{},
    });

    // A shot worth watching gets a swing to watch. The round decides that a
    // shot is long; which clip that corresponds to is the session's business,
    // since the round knows nothing of animation.
    if (update.event == game::RoundEvent::long_shot_struck &&
        session.character_player && session.follow_through_clip) {
        engine.play_clip(*session.character_player, *session.follow_through_clip);
    } else if (update.event == game::RoundEvent::ball_came_to_rest &&
               session.character_player && session.idle_clip) {
        // Back to idle when the ball lands rather than when the player
        // reaches it, or they walk the length of the fairway still frozen
        // in their finish.
        engine.play_clip(*session.character_player, *session.idle_clip);
    }

    // Stand the character where the round says the player is. Without this
    // the swing being watched and the swing being simulated happen in two
    // different places.
    if (session.character) {
        constexpr float degrees_to_radians = 3.14159265F / 180.0F;
        const auto facing = Quaternion::from_axis_angle(
            {0.0F, 1.0F, 0.0F},
            (update.player_facing_degrees + session.character_facing_offset_degrees) *
                degrees_to_radians);
        const auto upright = session.character_z_up
            ? Quaternion::from_axis_angle({1.0F, 0.0F, 0.0F}, -90.0F * degrees_to_radians)
            : Quaternion{};
        Transform placement;
        placement.set_position(update.player_position)
            .set_rotation(facing * upright)
            .set_uniform_scale(session.character_scale);
        engine.enqueue(SetEntityTransformCommand{*session.character, placement});
    }

    // Socket the club to the hand. The joint transform is read after the
    // character's own placement is decided, so the two agree on the frame
    // rather than the club trailing the hand by one.
    if (session.club_entity && session.club && session.character_player &&
        session.character) {
        const auto& club = *session.club;
        if (const auto joint =
                engine.joint_transform(*session.character_player, club.grip_joint)) {
            constexpr float degrees_to_radians = 3.14159265F / 180.0F;
            // The offset is in the club's own units and is applied before
            // the club is turned and scaled, so it can be read off the
            // model -- "put z = 3 in the hand" -- rather than being a
            // post-rotation nudge that has to be re-derived every time the
            // rotation changes.
            Transform shift;
            shift.set_position(club.grip_offset);
            Transform placement;
            placement.set_rotation(
                    Quaternion::from_axis_angle(
                        {0.0F, 1.0F, 0.0F}, club.grip_rotation_degrees.y * degrees_to_radians) *
                    Quaternion::from_axis_angle(
                        {1.0F, 0.0F, 0.0F}, club.grip_rotation_degrees.x * degrees_to_radians) *
                    Quaternion::from_axis_angle(
                        {0.0F, 0.0F, 1.0F}, club.grip_rotation_degrees.z * degrees_to_radians))
                .set_uniform_scale(club.scale);
            const auto grip = multiply(placement.matrix(), shift.matrix());
            const auto world = multiply(
                multiply(engine.transform(*session.character).matrix(), joint->matrix()), grip);
            engine.enqueue(
                SetEntityTransformCommand{*session.club_entity, Transform::from_matrix(world)});
        }
    }

    if (update.directs_camera) {
        auto camera = engine.camera();
        camera.look_at(update.eye, update.look_at);
        camera.set_perspective(
            description.viewpoint.vertical_field_of_view_degrees,
            description.viewpoint.near_plane,
            description.viewpoint.far_plane);
        engine.set_camera(std::move(camera));
    }
    return update.event;
}

void play_test_shot(Engine& engine, const CourseSession& session) {
    if (!session.round) {
        return;
    }
    const auto lie = engine.transform(session.ball).position();
    engine.submit_shot(golf::FullSwingData{
        .ball_speed_mps = 55.0F,
        .launch_angle_deg = 15.0F,
        .launch_direction_deg = session.round->round.aim_bearing_degrees(lie),
        .total_spin_rpm = 3'000.0F,
        .spin_axis_deg = 0.0F,
    });
}

game::RoundEvent toggle_range_finder(Engine& engine, const CourseSession& session) {
    if (!session.round) {
        return game::RoundEvent::none;
    }
    const auto lie = engine.transform(session.ball).position();
    return session.round->round.toggle_range_finder(
        {.elapsed_seconds = engine.last_tick_seconds(), .ball_position = lie, .ball_velocity = {}});
}

std::optional<float> range_find(Engine& engine, const CourseSession& session) {
    if (!session.round) {
        return std::nullopt;
    }
    const auto direction = session.round->round.sight_direction();
    if (!direction) {
        return std::nullopt;
    }
    const auto hit = engine.raycast(physics::Position{engine.camera().position()}, *direction);
    return hit ? std::optional<float>{hit->distance} : std::nullopt;
}

bool stream_course(
    Engine& engine,
    CourseSession& session,
    const CourseSessionDescription& description) {
    const auto eye = engine.camera().position();
    const Vec2 ground{eye.x, eye.z};
    auto rebuilt = false;

    auto tessellation = description.tessellation;
    tessellation.centre = ground;
    const auto wanted = snapped_terrain_centre(tessellation);
    if (wanted != session.terrain_centre) {
        if (engine.update_mesh(
                session.terrain, make_course_terrain(description.terrain, tessellation))) {
            session.terrain_centre = wanted;
            rebuilt = true;
        }
    }

    // The collidable patch follows the ball, not the camera: the ball is the
    // only thing in the world that touches the ground, and in flight it can
    // be two hundred metres from the player.
    const auto lie = engine.transform(session.ball).position();
    const Vec2 ball_ground{lie.x, lie.z};
    const auto strayed_x = ball_ground.x - session.collision_centre.x;
    const auto strayed_z = ball_ground.y - session.collision_centre.y;
    const auto allowed =
        description.collision.half_extent * std::clamp(description.collision.rebuild_fraction, 0.05F, 0.9F);
    if (strayed_x * strayed_x + strayed_z * strayed_z > allowed * allowed) {
        const auto wanted_centre = snapped_collision_centre(description.collision, ball_ground);
        if (wanted_centre != session.collision_centre) {
            engine.set_static_collider(
                session.ground,
                collision_patch(description.collision, description.terrain, ball_ground),
                collision_origin(description.collision, ball_ground));
            session.collision_centre = wanted_centre;
            rebuilt = true;
        }
    }

    if (session.grass && description.grass.radius > 0.0F && description.grass.density > 0.0F) {
        // Blades are rebuilt far less often than the terrain: scattering a
        // hundred thousand of them is the expensive half, and the field is
        // wide enough that it need only move once the camera has crossed a
        // good part of it.
        const auto moved_x = ground.x - session.grass_centre.x;
        const auto moved_z = ground.y - session.grass_centre.y;
        const auto threshold = description.grass.radius * 0.35F;
        if (moved_x * moved_x + moved_z * moved_z > threshold * threshold) {
            auto field = description.grass;
            field.centre = ground;
            if (engine.update_mesh(*session.grass, make_grass_field(field, description.terrain))) {
                session.grass_centre = ground;
                rebuilt = true;
            }
        }
    }

    return rebuilt;
}

CourseSessionDescription default_course_session(const std::filesystem::path& asset_directory) {
    // Built by assignment rather than as a designated aggregate. Naming a
    // member in a designated initializer and giving it {} discards its
    // default member initializer rather than keeping it, so every field this
    // function does not care about would be silently zeroed.
    CourseSessionDescription description;
    description.assets.ball_model = asset_directory / "ball.obj";
    description.assets.shader_directory = asset_directory / "shaders";
    description.character = CourseCharacter{
        .model = asset_directory / "characters/quaternius_male_casual.glb",
        .skeleton = asset_directory / "characters/quaternius_male_casual_skeleton.ozz",
        .animation = asset_directory / "characters/quaternius_male_casual_idle.ozz",
        .follow_through = asset_directory / "characters/quaternius_male_casual_finish.ozz",
        // Stand just left of the ball, facing up the hole. The source asset
        // is 4.84 Blender units tall, so this puts the golfer at 1.79 m.
        .position = {4.77F, -46.59F},
        .facing_degrees = -176.3F,
        .facing_offset_degrees = 180.0F,
        .scale = 0.37F,
        .club = CourseClub{.model = asset_directory / "golf_club.stl"},
        .z_up = false,
    };

    auto& environment = description.environment;
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

    return description;
}

float exposure_from_ev100(float ev100) noexcept {
    // The saturation-based speed relation: the luminance that saturates the
    // sensor is 1.2 * 2^EV100, and exposure is its reciprocal.
    return 1.0F / (1.2F * std::pow(2.0F, ev100));
}

} // namespace mgv
