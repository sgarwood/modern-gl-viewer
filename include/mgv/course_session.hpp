#pragma once

#include "mgv/engine.hpp"
#include "mgv/game/round.hpp"
#include "mgv/lighting.hpp"
#include "mgv/primitives.hpp"
#include "mgv/renderer.hpp"

#include <filesystem>
#include <memory>
#include <optional>

namespace mgv {

/// How a course session should be framed when it starts.
///
/// Positions are given in the ground plane with a height above the terrain,
/// not as absolute points. The hole falls about two metres over its length,
/// so an absolute eye height that works at the green puts the camera
/// underground on the fairway.
///
/// The default is a third-person view over the player's rear quarter, with
/// enough of the approach ahead to read the lie and the line to the green.
struct CourseViewpoint final {
    /// Where the camera stands, in the ground plane.
    Vec2 position{8.5F, -50.0F};
    /// Eye height above the terrain, in metres. The default stands the camera
    /// above and behind the golfer without turning into a bird's-eye view.
    float eye_height{2.8F};
    /// What the camera looks at, in the ground plane.
    Vec2 target{4.6F, -44.0F};
    /// Height above the terrain at the target, in metres.
    float target_height{0.9F};
    float vertical_field_of_view_degrees{42.0F};
    float near_plane{0.05F};
    float far_plane{4'000.0F};
};

/// A skinned character to stand on the course.
///
/// All three parts are needed: the rigged mesh, and the skeleton and clip it
/// is driven by. The skeleton and clip are converted separately -- by
/// `gltf2ozz` -- which is exactly why the skin binds to them by joint name.
/// A prop held in a character's hand.
struct CourseClub final {
    /// Binary STL. A club comes out of CAD or a sculpting tool, which is
    /// what STL is for; it carries no materials, so the session gives it
    /// one.
    std::filesystem::path model;
    /// The joint it is gripped by, by name. The lead hand, for a club: the
    /// trail hand follows it rather than the other way round.
    std::string grip_joint{"Palm.L"};
    /// Metres per model unit.
    float scale{0.42F};
    /// Which point of the club lands in the hand, in the club's own units
    /// and applied before it is turned and scaled, and how it is then
    /// turned to lie in the grip. Authored against the asset, since no two
    /// exporters agree on which way a club points.
    /// Defaults authored against assets/golf_club.stl held in the shipped
    /// character's Palm.L. The club's shaft runs along +Z with the head at
    /// -Z and its origin sits mid-shaft, so z = 3 is the butt of the grip
    /// and belongs in the hand.
    Vec3 grip_offset{0.0F, 0.0F, -3.0F};
    Vec3 grip_rotation_degrees{90.0F, 0.0F, 0.0F};
};

struct CourseCharacter final {
    std::filesystem::path model;
    std::filesystem::path skeleton;
    /// The clip the character plays while nothing else is happening.
    std::filesystem::path animation;
    /// The clip played when a shot worth watching is struck. Optional: with
    /// none, the character simply keeps idling through the swing.
    std::filesystem::path follow_through;
    /// Entry point in the full swing clip when the launch monitor reports
    /// impact. Keeping the address and backswing in the asset makes it useful
    /// outside the simulator without delaying the simulated ball flight.
    animation::AnimationDuration follow_through_start{};
    std::filesystem::path walk;
    /// Where the character stands, in the ground plane.
    Vec2 position{};
    /// Compass bearing it faces, degrees.
    float facing_degrees{};
    /// Rotation from the model's authored forward axis to the engine's -Z
    /// forward axis. For example, Blender's -Y becomes glTF +Z and needs 180
    /// degrees here.
    float facing_offset_degrees{};
    /// Uniform scale. Characters are rarely authored in metres.
    float scale{1.0F};
    /// The club they are holding, if any.
    std::optional<CourseClub> club{};
    /// Whether the asset was authored Z-up and needs standing upright.
    ///
    /// glTF declares Y-up, but exports that began life in a Z-up tool
    /// frequently carry the correction on a node above the skeleton, which a
    /// skeleton converter has no reason to keep. The symptom is a character
    /// that renders flawlessly, lying on its back.
    bool z_up{};
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

/// The patch of ground the ball can actually collide with.
///
/// The drawn terrain runs to the horizon, but a heightmap fine enough to
/// putt on cannot cover that much ground: at the resolution a green needs,
/// the whole course would be millions of samples. The patch therefore
/// follows the ball, which is the only thing in the world that collides
/// with the ground.
struct CourseCollisionDescription final {
    /// Half-width of the patch, in metres. It only has to outrun the ball
    /// between rebuilds, and the physics clamps how far that can be in one
    /// tick, so this is generous.
    float half_extent{45.0F};
    /// Spacing between samples, in metres. The green's undulation has a
    /// wavelength of about sixteen metres, so this resolves it to well under
    /// a millimetre after interpolation.
    float resolution{0.4F};
    /// How far the ball may stray from the patch's centre before it is
    /// rebuilt, as a fraction of the half-extent.
    float rebuild_fraction{0.3F};
    /// Height of the backstop that catches anything leaving the patch.
    float backstop_height{-40.0F};
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
    /// A skinned character, if one is wanted.
    std::optional<CourseCharacter> character;
    CourseViewpoint viewpoint;
    Environment environment;
    /// Where the ball is teed or lying, in the ground plane. Its height comes
    /// from the terrain, so it always starts resting on the surface.
    Vec2 ball_start{4.6F, -44.0F};
    /// Where the cup is cut, in the ground plane. The pin, the collar, and
    /// the round's idea of "holed" all come from this one number.
    Vec2 hole{1.9F, -2.4F};
    /// How the round is played. Its hole is filled in from `hole`.
    game::RoundRules round;
};

/// A hole that has been built, and the handles needed to keep it up to date
/// as the player moves through it.
struct CourseSession final {
    EntityId ball{};
    EntityId terrain{};
    /// The streamed patch of collidable ground.
    physics::BodyId ground{};
    /// Where that patch is currently centred.
    Vec2 collision_centre{};
    /// Absent when the session was built with no blade field.
    std::optional<EntityId> grass{};
    /// Where the terrain's detail is currently centred, already snapped.
    Vec2 terrain_centre{};
    /// Where the blade field is currently centred.
    Vec2 grass_centre{};
    /// The character entity, if the session placed one, and whether its
    /// asset needed standing upright.
    std::optional<EntityId> character{};
    bool character_z_up{};
    float character_facing_offset_degrees{};
    float character_scale{1.0F};
    /// The club entity and how it is socketed, if the session placed one.
    std::optional<EntityId> club_entity{};
    std::optional<CourseClub> club{};
    /// The character's animation player, if the session placed one.
    std::optional<animation::AnimationPlayerId> character_player{};
    /// Its idle and follow-through clips.
    std::optional<animation::AnimationClipId> idle_clip{};
    std::optional<animation::AnimationClipId> follow_through_clip{};
    std::optional<animation::AnimationClipId> walk_clip{};
    /// The round being played, and the ground it walks over. Held by shared
    /// pointer because a session is passed around by value and the round
    /// holds a reference to its ground.
    std::shared_ptr<class CourseRound> round;
};

/// Advances the round and applies the camera it asks for.
///
/// Frontends call this each frame, alongside `stream_course`. The round is
/// deliberately not inside `Engine::tick`: the engine is a renderer runtime,
/// and the rules of golf are not its business.
/// What stage of the shot the session's round is at.
///
/// The round itself is held behind an opaque type so that callers do not
/// depend on how a course keeps its ground; this is the one fact about it
/// that a host needs often enough to be worth exposing.
[[nodiscard]] game::RoundState round_state(const CourseSession& session);

game::RoundEvent advance_round(
    Engine& engine,
    const CourseSession& session,
    const CourseSessionDescription& description);

/// Strikes the ball towards the hole with the deterministic test swing.
///
/// Which way the green lies is a property of the round, so this aims at it;
/// `InputAction::fire_test_shot` handled inside the engine fires straight
/// down -Z, which is a viewer debug command rather than a played shot.
void play_test_shot(Engine& engine, const CourseSession& session);

/// Raises or stows the range finder.
game::RoundEvent toggle_range_finder(Engine& engine, const CourseSession& session);

/// Ranges whatever the range finder is pointing at, in metres. Nothing when
/// it is stowed or nothing is in the way.
[[nodiscard]] std::optional<float> range_find(Engine& engine, const CourseSession& session);

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
