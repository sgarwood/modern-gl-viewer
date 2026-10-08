#include "mgv/game/round.hpp"

#include <algorithm>
#include <cmath>
#include <utility>

namespace mgv::game {
namespace {

[[nodiscard]] Vec3 add(Vec3 lhs, Vec3 rhs) noexcept {
    return {lhs.x + rhs.x, lhs.y + rhs.y, lhs.z + rhs.z};
}

[[nodiscard]] Vec3 subtract(Vec3 lhs, Vec3 rhs) noexcept {
    return {lhs.x - rhs.x, lhs.y - rhs.y, lhs.z - rhs.z};
}

[[nodiscard]] Vec3 scaled(Vec3 value, float factor) noexcept {
    return {value.x * factor, value.y * factor, value.z * factor};
}

[[nodiscard]] float length(Vec3 value) noexcept {
    return std::sqrt(value.x * value.x + value.y * value.y + value.z * value.z);
}

[[nodiscard]] Vec3 normalized(Vec3 value, Vec3 fallback) noexcept {
    const auto magnitude = length(value);
    return magnitude > 1.0e-5F ? scaled(value, 1.0F / magnitude) : fallback;
}

[[nodiscard]] Vec3 cross(Vec3 lhs, Vec3 rhs) noexcept {
    return {
        lhs.y * rhs.z - lhs.z * rhs.y,
        lhs.z * rhs.x - lhs.x * rhs.z,
        lhs.x * rhs.y - lhs.y * rhs.x,
    };
}

/// Smooth start and stop, so the walk does not jerk into motion or stop dead.
[[nodiscard]] float ease(float t) noexcept {
    const auto clamped = std::clamp(t, 0.0F, 1.0F);
    return clamped * clamped * (3.0F - 2.0F * clamped);
}

} // namespace

struct Round::Impl final {
    Impl(RoundRules rules_in, const GroundHeights& ground_in)
        : rules{rules_in}, ground{&ground_in} {}

    /// Puts a point the given height above the ground beneath it, so no
    /// camera ever walks through a hill.
    [[nodiscard]] Vec3 on_ground(Vec3 point, float height) const {
        return {point.x, ground->height_at(point.x, point.z) + height, point.z};
    }

    [[nodiscard]] Vec3 hole_position() const {
        return on_ground({rules.hole.x, 0.0F, rules.hole.y}, 0.0F);
    }

    /// Where a player stands to address a ball: behind it on the line to the
    /// hole, looking down that line.
    [[nodiscard]] Vec3 stance_for(Vec3 ball) const {
        const auto to_hole = subtract(hole_position(), ball);
        const Vec3 back{-to_hole.x, 0.0F, -to_hole.z};
        const auto behind = normalized(back, {0.0F, 0.0F, 1.0F});
        return on_ground(add(ball, scaled(behind, rules.stance_distance)), rules.eye_height);
    }

    RoundRules rules;
    const GroundHeights* ground;

    RoundState state{RoundState::addressing};
    /// The state to return to when the range finder is stowed.
    RoundState stowed_state{RoundState::addressing};
    float timer{};
    float sway_time{};
    Vec3 walk_from{};
    Vec3 walk_to{};
    Vec3 sight_from{};
    Vec3 sight_at{};
    bool sighting{};
};

Round::Round(RoundRules rules, const GroundHeights& ground)
    : impl_{std::make_unique<Impl>(rules, ground)} {}
Round::~Round() = default;
Round::Round(Round&&) noexcept = default;
Round& Round::operator=(Round&&) noexcept = default;

RoundState Round::state() const noexcept {
    return impl_->state;
}

const RoundRules& Round::rules() const noexcept {
    return impl_->rules;
}

void Round::set_rules(RoundRules rules) {
    impl_->rules = rules;
}

RoundEvent Round::toggle_range_finder(const RoundObservation& observation) {
    if (impl_->sighting) {
        impl_->sighting = false;
        impl_->state = impl_->stowed_state;
        return RoundEvent::range_finder_stowed;
    }
    impl_->sighting = true;
    impl_->stowed_state = impl_->state;
    impl_->state = RoundState::sighting;
    impl_->sway_time = 0.0F;
    impl_->sight_from = impl_->stance_for(observation.ball_position);
    impl_->sight_at = impl_->hole_position();
    return RoundEvent::range_finder_deployed;
}

float Round::aim_bearing_degrees(Vec3 ball) const {
    const auto hole = impl_->hole_position();
    const auto across_x = hole.x - ball.x;
    const auto across_z = hole.z - ball.z;
    if (std::abs(across_x) < 1.0e-5F && std::abs(across_z) < 1.0e-5F) {
        return 0.0F;
    }
    constexpr auto radians_to_degrees = 57.29577951F;
    return std::atan2(across_x, -across_z) * radians_to_degrees;
}

std::optional<Vec3> Round::sight_direction() const {
    if (!impl_->sighting) {
        return std::nullopt;
    }
    return normalized(subtract(impl_->sight_at, impl_->sight_from), {0.0F, 0.0F, -1.0F});
}

RoundUpdate Round::update(const RoundObservation& observation) {
    auto& self = *impl_;
    const auto& rules = self.rules;
    const auto ball = observation.ball_position;
    auto event = RoundEvent::none;

    if (self.sighting) {
        // The range finder is hand-held, so it wanders. Two incommensurate
        // frequencies per axis, which never settle into a visible rhythm the
        // way one would.
        self.sway_time += observation.elapsed_seconds;
        const auto forward = normalized(subtract(self.sight_at, self.sight_from), {0.0F, 0.0F, -1.0F});
        const auto right = normalized(cross(forward, {0.0F, 1.0F, 0.0F}), {1.0F, 0.0F, 0.0F});
        const auto up = cross(right, forward);
        const auto yaw = std::sin(self.sway_time * 1.5F) * rules.sight_sway +
                         std::cos(self.sway_time * 0.83F) * rules.sight_sway * 0.5F;
        const auto pitch = std::cos(self.sway_time * 1.21F) * rules.sight_sway * 0.8F +
                           std::sin(self.sway_time * 0.47F) * rules.sight_sway * 0.4F;
        const auto aim = add(self.sight_at, add(scaled(right, yaw), scaled(up, pitch)));
        return {
            .state = RoundState::sighting,
            .event = event,
            .directs_camera = true,
            .eye = self.sight_from,
            .look_at = aim,
        };
    }

    switch (self.state) {
    case RoundState::addressing:
        if (observation.ball_speed > rules.struck_speed) {
            self.state = RoundState::in_flight;
            event = RoundEvent::ball_struck;
        }
        break;

    case RoundState::in_flight:
        if (observation.ball_speed <= rules.at_rest_speed) {
            const auto hole = self.hole_position();
            const auto across = std::sqrt(
                (ball.x - hole.x) * (ball.x - hole.x) + (ball.z - hole.z) * (ball.z - hole.z));
            self.timer = 0.0F;
            if (across <= rules.hole_radius) {
                self.state = RoundState::holing_out;
                event = RoundEvent::ball_holed;
            } else {
                self.state = RoundState::walking;
                self.walk_from = self.on_ground(
                    {self.walk_from.x, 0.0F, self.walk_from.z}, rules.eye_height);
                self.walk_to = self.stance_for(ball);
                event = RoundEvent::ball_came_to_rest;
            }
        }
        break;

    case RoundState::walking: {
        self.timer += observation.elapsed_seconds;
        if (self.timer >= std::max(rules.walk_duration, 1.0e-3F)) {
            self.state = RoundState::addressing;
            event = RoundEvent::reached_ball;
        }
        break;
    }

    case RoundState::holing_out:
        self.timer += observation.elapsed_seconds;
        if (self.timer >= rules.holing_out_duration) {
            self.state = RoundState::addressing;
        }
        break;

    case RoundState::sighting:
        break;
    }

    // --- where the camera goes ----------------------------------------------
    switch (self.state) {
    case RoundState::in_flight: {
        // Behind and above the ball, following it down.
        const auto behind = self.on_ground(
            {ball.x, 0.0F, ball.z + rules.stance_distance * 2.2F}, rules.eye_height + 1.4F);
        self.walk_from = behind;
        return {
            .state = self.state,
            .event = event,
            .directs_camera = true,
            .eye = behind,
            .look_at = ball,
        };
    }
    case RoundState::walking: {
        const auto t = ease(self.timer / std::max(rules.walk_duration, 1.0e-3F));
        auto eye = add(scaled(self.walk_from, 1.0F - t), scaled(self.walk_to, t));
        // Follow the ground rather than interpolating through it, and bob
        // with the stride.
        eye = self.on_ground(eye, rules.eye_height);
        eye.y += std::sin(self.timer * 9.0F) * rules.walk_bob;
        return {
            .state = self.state,
            .event = event,
            .directs_camera = true,
            .eye = eye,
            .look_at = ball,
        };
    }
    case RoundState::holing_out: {
        const auto hole = self.hole_position();
        const auto t = ease(self.timer / std::max(rules.holing_out_duration, 1.0e-3F));
        const auto standing = self.stance_for(hole);
        const auto over = self.on_ground(
            {hole.x, 0.0F, hole.z + 0.55F}, rules.eye_height * 0.55F);
        const auto eye = add(scaled(standing, 1.0F - t), scaled(over, t));
        return {
            .state = self.state,
            .event = event,
            .directs_camera = true,
            .eye = eye,
            .look_at = hole,
        };
    }
    case RoundState::addressing:
    case RoundState::sighting:
    default: {
        const auto eye = self.stance_for(ball);
        self.walk_from = eye;
        // Addressing is the player's: they are lining up, and the view is
        // theirs until the ball is struck.
        return {
            .state = self.state,
            .event = event,
            .directs_camera = false,
            .eye = eye,
            .look_at = ball,
        };
    }
    }
}

} // namespace mgv::game
