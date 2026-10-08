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
    /// The ball is moving.
    in_flight,
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
    /// Eye height above the ground, in metres.
    float eye_height{1.62F};
    /// How far behind the ball the player stands to address it.
    float stance_distance{2.6F};
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
    /// Speed of the ball, in m/s.
    float ball_speed{};
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
    void set_rules(RoundRules rules);

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace mgv::game
