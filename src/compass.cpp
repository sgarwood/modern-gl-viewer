#include "mgv/compass.hpp"

#include <cmath>
#include <numbers>

namespace mgv {
namespace {

constexpr float degrees_to_radians = std::numbers::pi_v<float> / 180.0F;
constexpr float radians_to_degrees = 180.0F / std::numbers::pi_v<float>;

} // namespace

Vec3 bearing_to_direction(float degrees) noexcept {
    const auto radians = degrees * degrees_to_radians;
    return {std::sin(radians), 0.0F, -std::cos(radians)};
}

float direction_to_bearing(Vec3 direction) noexcept {
    if (std::abs(direction.x) < 1.0e-6F && std::abs(direction.z) < 1.0e-6F) {
        return 0.0F;
    }
    return std::atan2(direction.x, -direction.z) * radians_to_degrees;
}

} // namespace mgv
