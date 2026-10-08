#pragma once

#include "mgv/mesh.hpp"

namespace mgv {

/// Converts a compass bearing in degrees to a horizontal unit vector.
///
/// Zero is -Z and the angle runs clockwise, which is the meteorological
/// convention and the one the golf shot model launches along. Written down
/// once, here, because it had been open-coded three times -- for the sun,
/// for the wind, and for aiming a shot -- and a sign error in any of them
/// would have been invisible until something flew the wrong way.
[[nodiscard]] Vec3 bearing_to_direction(float degrees) noexcept;

/// The inverse: the bearing of a horizontal direction, in degrees, in
/// [-180, 180]. The vertical component is ignored.
[[nodiscard]] float direction_to_bearing(Vec3 direction) noexcept;

} // namespace mgv
