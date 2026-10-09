#include "mgv/physics/aerodynamics.hpp"

#include <algorithm>
#include <cmath>

namespace mgv::physics {
namespace {

/// The measured band of the drag fit's transition term, in Reynolds number.
constexpr float transition_onset = 90'000.0F;
constexpr float transition_width = 200'000.0F;

/// Smits and Smith's spin-down constant, 4.0e-6 with speed in miles per hour,
/// converted to metres per second: 4.0e-6 * 2.236936. Dimensionless, so the
/// rate it gives is per second for a speed and a radius both in SI.
constexpr float spin_down_constant = 8.9477e-6F;

/// Sea level air density, the density the published spin-down rate was
/// measured at.
constexpr float reference_air_density = 1.225F;

} // namespace

float spin_ratio(float speed, float spin, float radius) noexcept {
    if (speed <= 0.0F || radius <= 0.0F) {
        return 0.0F;
    }
    return std::abs(spin) * radius / speed;
}

float reynolds_number(float speed, float radius, float air_density) noexcept {
    if (speed <= 0.0F || radius <= 0.0F || air_density <= 0.0F) {
        return 0.0F;
    }
    const auto kinematic_viscosity = air_dynamic_viscosity / air_density;
    return speed * 2.0F * radius / kinematic_viscosity;
}

float drag_coefficient(float spin_ratio, float reynolds_number) noexcept {
    const auto transition = std::clamp(
        (reynolds_number - transition_onset) / transition_width, 0.0F, 1.0F);
    return 0.24F + (0.18F * spin_ratio) +
        (0.06F * std::sin(3.14159265F * transition));
}

float lift_coefficient(float spin_ratio) noexcept {
    if (spin_ratio <= 0.0F) {
        return 0.0F;
    }
    return 0.54F * std::pow(spin_ratio, 0.4F);
}

float spin_decay_rate(float speed, float radius, float air_density) noexcept {
    if (speed <= 0.0F || radius <= 0.0F || air_density <= 0.0F) {
        return 0.0F;
    }
    return spin_down_constant * (air_density / reference_air_density) * speed / radius;
}

} // namespace mgv::physics
