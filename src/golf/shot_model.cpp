#include "mgv/golf/shot_model.hpp"

#include <cmath>
#include <numbers>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <variant>

namespace mgv::golf {
namespace {

constexpr float degrees_to_radians = std::numbers::pi_v<float> / 180.0F;
constexpr float rpm_to_radians_per_second = 2.0F * std::numbers::pi_v<float> / 60.0F;

void require_finite(float value, const char* name) {
    if (!std::isfinite(value)) {
        throw std::invalid_argument{std::string{name} + " must be finite"};
    }
}

void require_range(float value, float minimum, float maximum, const char* name) {
    require_finite(value, name);
    if (value < minimum || value > maximum) {
        throw std::invalid_argument{std::string{name} + " is outside its valid range"};
    }
}

void validate(const FullSwingData& shot) {
    require_finite(shot.ball_speed_mps, "Ball speed");
    require_range(shot.launch_angle_deg, -90.0F, 90.0F, "Launch angle");
    require_range(shot.launch_direction_deg, -180.0F, 180.0F, "Launch direction");
    require_finite(shot.total_spin_rpm, "Total spin");
    require_range(shot.spin_axis_deg, -180.0F, 180.0F, "Spin axis");
    if (shot.ball_speed_mps < 0.0F || shot.total_spin_rpm < 0.0F) {
        throw std::invalid_argument{"Ball speed and total spin cannot be negative"};
    }
}

void validate(const PuttingData& shot) {
    require_finite(shot.putter_speed_mps, "Putter speed");
    require_range(shot.face_angle_deg, -180.0F, 180.0F, "Face angle");
    require_range(shot.twist_deg, -180.0F, 180.0F, "Twist");
    require_range(shot.lie_angle_deg, 0.0F, 180.0F, "Lie angle");
    require_range(shot.shaft_lean_deg, -90.0F, 90.0F, "Shaft lean");
    require_range(shot.loft_angle_deg, -90.0F, 90.0F, "Loft angle");
    if (shot.putter_speed_mps < 0.0F) {
        throw std::invalid_argument{"Putter speed cannot be negative"};
    }
}

[[nodiscard]] Vec3 horizontal_forward(float direction) {
    return {std::sin(direction), 0.0F, -std::cos(direction)};
}

[[nodiscard]] Vec3 horizontal_right(float direction) {
    return {std::cos(direction), 0.0F, std::sin(direction)};
}

[[nodiscard]] BallLaunch resolve_full_swing(const FullSwingData& shot) {
    validate(shot);
    const auto launch = shot.launch_angle_deg * degrees_to_radians;
    const auto direction = shot.launch_direction_deg * degrees_to_radians;
    const auto spin_axis = shot.spin_axis_deg * degrees_to_radians;
    const auto horizontal_speed = shot.ball_speed_mps * std::cos(launch);
    const auto forward = horizontal_forward(direction);
    const auto right = horizontal_right(direction);
    const auto spin = shot.total_spin_rpm * rpm_to_radians_per_second;

    return {
        .linear_velocity = physics::LinearVelocity{{
            forward.x * horizontal_speed,
            shot.ball_speed_mps * std::sin(launch),
            forward.z * horizontal_speed,
        }},
        .angular_velocity = physics::AngularVelocity{{
            right.x * spin * std::cos(spin_axis),
            spin * std::sin(spin_axis),
            right.z * spin * std::cos(spin_axis),
        }},
    };
}

} // namespace

StandardShotModel::StandardShotModel() = default;

StandardShotModel::StandardShotModel(StandardShotModelConfiguration configuration)
    : configuration_{configuration} {
    if (!std::isfinite(configuration_.putting_smash_factor) ||
        configuration_.putting_smash_factor <= 0.0F) {
        throw std::invalid_argument{"Putting smash factor must be finite and positive"};
    }
}

BallLaunch StandardShotModel::resolve(const Shot& shot) const {
    return std::visit(
        [this](const auto& value) -> BallLaunch {
            using Value = std::remove_cvref_t<decltype(value)>;
            if constexpr (std::is_same_v<Value, FullSwingData>) {
                return resolve_full_swing(value);
            } else {
                validate(value);
                const auto direction = value.face_angle_deg * degrees_to_radians;
                const auto forward = horizontal_forward(direction);
                const auto right = horizontal_right(direction);
                const auto speed = value.putter_speed_mps * configuration_.putting_smash_factor;
                const auto rolling_speed = speed / configuration_.ball_radius.metres();
                return {
                    .linear_velocity = physics::LinearVelocity{{
                        forward.x * speed,
                        0.0F,
                        forward.z * speed,
                    }},
                    .angular_velocity = physics::AngularVelocity{{
                        right.x * rolling_speed,
                        0.0F,
                        right.z * rolling_speed,
                    }},
                };
            }
        },
        shot);
}

} // namespace mgv::golf
