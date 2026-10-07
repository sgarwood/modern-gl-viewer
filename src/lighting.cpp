#include "mgv/lighting.hpp"

#include <cmath>
#include <numbers>

namespace mgv {
namespace {

constexpr Vec3 default_direction{0.4131759F, 0.7660444F, 0.4924039F};
constexpr float degrees_to_radians = std::numbers::pi_v<float> / 180.0F;

} // namespace

Vec3 normalized_light_direction(Vec3 direction) noexcept {
    const auto length = std::sqrt(
        direction.x * direction.x + direction.y * direction.y + direction.z * direction.z);
    if (!(length > 1.0e-6F)) {
        return default_direction;
    }
    return {direction.x / length, direction.y / length, direction.z / length};
}

Vec3 sun_direction_from_angles(float azimuth_degrees, float elevation_degrees) noexcept {
    const auto azimuth = azimuth_degrees * degrees_to_radians;
    const auto elevation = elevation_degrees * degrees_to_radians;
    const auto horizontal = std::cos(elevation);
    return normalized_light_direction({
        horizontal * std::sin(azimuth),
        std::sin(elevation),
        -horizontal * std::cos(azimuth),
    });
}

} // namespace mgv
