#pragma once

#include "mgv/camera.hpp"
#include "mgv/mesh.hpp"

#include <memory>
#include <optional>

namespace mgv::game {

/// Where a round of golf currently is.
enum class RoundState {
    /// Standing over a ball at rest, waiting for it to be struck.
    addressing,
    /// The ball has just been struck hard, and the camera is held on the
    /// player while they finish their swing.
    following_through,
    /// The ball is moving.
    in_flight,
    /// The camera has pulled back and is following the ball down its flight.
    tracing,
    /// Following the ball to wherever it came to rest.
    walking,
    /// The ball is in the cup and the camera is looking into it.
    holing_out,
    /// Looking down the hole through the range finder.
    sighting,
};

/// Something the round decided happened, for a frontend to present however it
/// presents things.
///
/// A value rather than a line of console output: the engine has no business
/// deciding that the way to tell a player their ball is holed is to write to
/// standard output.
enum class RoundEvent {
    none,
    ball_struck,
    /// Struck far enough to be worth watching: the player finishes their
    /// swing and the camera pulls back to follow the ball.
    long_shot_struck,
    ball_came_to_rest,
    ball_holed,
    reached_ball,
    range_finder_deployed,
    range_finder_stowed,
};

/// The ground the round walks the camera over.
///
/// A port rather than the terrain itself: the rules of a round do not depend
/// on how the ground is generated, and a test wants a hill it can describe in
/// two lines rather than a whole course.
class GroundHeights {
public:
    virtual ~GroundHeights() = default;
    GroundHeights(const GroundHeights&) = delete;
    GroundHeights& operator=(const GroundHeights&) = delete;

    [[nodiscard]] virtual float height_at(float x, float z) const = 0;

protected:
    GroundHeights() = default;
};

/// The numbers a round is played by. All of them, in one place, rather than
/// spread through a tick function as literals.
struct RoundRules final {
    /// Where the cup is, in the ground plane.
    Vec2 hole{};
    /// A regulation cup is 108 mm across.
    float hole_radius{0.054F};
    /// At or below this speed the ball counts as at rest, in m/s.
    float at_rest_speed{0.05F};
    /// Above this speed it counts as struck, in m/s.
    float struck_speed{1.0F};
    /// A shot estimated to carry at least this far is worth watching, in
    /// metres. A hundred and twenty yards.
    float long_shot_distance{109.7F};
    /// How long the camera holds on the player's finish before it pulls
    /// back, in seconds.
    float follow_through_duration{1.15F};
    /// How long the pull-back takes once it starts, in seconds.
    float trace_pull_back_duration{1.4F};
    /// How far behind the player the tracing camera settles, in metres.
    float trace_distance{17.0F};
    /// How high above the player it settles, in metres.
    float trace_height{7.5F};
    /// How far to the side the camera stands to watch the finish, in metres.
    /// Far enough to see the whole turn, close enough that it reads.
    float follow_through_distance{4.6F};
    /// Eye height above the ground, in metres.
    float eye_height{1.62F};
    /// How far behind the ball the player stands to address it (5ft).
    float stance_distance{1.524F};
    /// How long the walk to the ball takes, in seconds.
    float walk_duration{4.0F};
    /// How long the camera lingers over the cup, in seconds.
    float holing_out_duration{2.0F};
    /// Amplitude of the hands-free wander through the range finder, radians.
    float sight_sway{0.045F};
    /// Height of the bob in the camera as the player walks, in metres.
    float walk_bob{0.055F};
};

/// What the round observed of the world this frame.
struct RoundObservation final {
    /// Seconds since the previous update.
    float elapsed_seconds{};
    Vec3 ball_position{};
    /// Velocity of the ball, in m/s.
    ///
    /// The whole vector, not just its magnitude: how far a struck ball will
    /// carry depends on how steeply it left, and the round has to decide
    /// whether a shot is worth watching at the moment of impact, long before
    /// the physics has an answer.
    Vec3 ball_velocity{};
};

/// What the round decided, and where it wants the camera.
struct RoundUpdate final {
    RoundState state{RoundState::addressing};
    RoundEvent event{RoundEvent::none};
    /// Whether the round is driving the camera this frame.
    ///
    /// False while addressing the ball: the player is lining up a shot and
    /// the view is theirs. True for the states the round is staging -- the
    /// flight, the walk, the ball dropping in -- where it is the director.
    bool directs_camera{};
    /// Where the round would put the camera. Always filled in, so a caller
    /// that wants the stance camera anyway can have it.
    Vec3 eye{};
    Vec3 look_at{};
    /// Where the player is standing, on the ground, and the compass bearing
    /// they face.
    ///
    /// The round already has to know this to place the camera, and a third
    /// person view needs the character to stand in the same place the round
    /// thinks they do -- otherwise the swing being watched and the swing
    /// being simulated are two different events.
    Vec3 player_position{};
    float player_facing_degrees{};
};

/// The state machine for a round of golf: what is happening, and where the
/// camera should be while it happens.
///
/// Deliberately knows nothing of the renderer, the physics world, or the
/// engine. It takes what it observed and returns what it decided, which is
/// what makes it testable without a window.
class Round final {
public:
    Round(RoundRules rules, const GroundHeights& ground);
    ~Round();

    Round(Round&&) noexcept;
    Round& operator=(Round&&) noexcept;
    Round(const Round&) = delete;
    Round& operator=(const Round&) = delete;

    [[nodiscard]] RoundUpdate update(const RoundObservation& observation);

    /// Raises or stows the range finder. Returns the resulting event.
    RoundEvent toggle_range_finder(const RoundObservation& observation);

    /// The compass bearing from a ball to the hole, in degrees, in the
    /// convention the shot model launches along: zero is -Z, measured
    /// clockwise. Which way the green lies is a fact about the round, not
    /// about the engine that moves the ball.
    [[nodiscard]] float aim_bearing_degrees(Vec3 ball) const;

    /// The line the range finder is currently pointing along, if it is up.
    /// A caller with a physics world turns this into a yardage.
    [[nodiscard]] std::optional<Vec3> sight_direction() const;

    [[nodiscard]] RoundState state() const noexcept;
    [[nodiscard]] const RoundRules& rules() const noexcept;

    /// Roughly how far a ball leaving at this velocity will carry, in
    /// metres.
    ///
    /// The vacuum range. A real golf ball's backspin generates lift and
    /// carries it further, so this understates a struck shot, and it is
    /// used only to decide which camera to use -- never for anything the
    /// physics is responsible for.
    [[nodiscard]] static float estimated_carry(Vec3 velocity) noexcept;
    void set_rules(RoundRules rules);

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace mgv::game
