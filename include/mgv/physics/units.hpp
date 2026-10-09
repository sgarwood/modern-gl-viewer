#pragma once

#include "mgv/mesh.hpp"

namespace mgv::physics {

class Duration final {
public:
    explicit Duration(float seconds);
    [[nodiscard]] float seconds() const noexcept;

private:
    float seconds_{};
};

class Length final {
public:
    explicit Length(float metres);
    [[nodiscard]] float metres() const noexcept;

private:
    float metres_{};
};

class Mass final {
public:
    explicit Mass(float kilograms);
    [[nodiscard]] float kilograms() const noexcept;

private:
    float kilograms_{1.0F};
};

class Position final {
public:
    Position() = default;
    explicit Position(Vec3 metres);
    [[nodiscard]] const Vec3& metres() const noexcept;

private:
    Vec3 metres_{};
};

class Dimensions final {
public:
    explicit Dimensions(Vec3 metres);
    [[nodiscard]] const Vec3& metres() const noexcept;

private:
    Vec3 metres_{};
};

class LinearVelocity final {
public:
    LinearVelocity() = default;
    explicit LinearVelocity(Vec3 metres_per_second);
    [[nodiscard]] const Vec3& metres_per_second() const noexcept;

private:
    Vec3 metres_per_second_{};
};

class Acceleration final {
public:
    Acceleration() = default;
    explicit Acceleration(Vec3 metres_per_second_squared);
    [[nodiscard]] const Vec3& metres_per_second_squared() const noexcept;

private:
    Vec3 metres_per_second_squared_{};
};

class AngularVelocity final {
public:
    AngularVelocity() = default;
    explicit AngularVelocity(Vec3 radians_per_second);
    [[nodiscard]] const Vec3& radians_per_second() const noexcept;

private:
    Vec3 radians_per_second_{};
};

class Impulse final {
public:
    explicit Impulse(Vec3 newton_seconds);
    [[nodiscard]] const Vec3& newton_seconds() const noexcept;

private:
    Vec3 newton_seconds_{};
};

/// Standard atmospheric pressure at sea level, in pascals.
inline constexpr float standard_sea_level_pressure = 101'325.0F;

/// Density of air, in kilograms per cubic metre.
///
/// Carry distance is roughly proportional to it, so this is not a detail: a
/// course a mile up plays several percent longer than the same course at the
/// coast, and that difference comes entirely from this function. Taking the
/// pressure as sea level regardless of where the round is played throws that
/// away.
///
/// `relative_humidity` runs 0 to 1. Humid air is *less* dense than dry air at
/// the same temperature and pressure, because a water molecule is lighter
/// than the nitrogen and oxygen it displaces -- which is the opposite of what
/// most people expect, and worth the few lines it costs to get right.
/// Air pressure at an elevation, in pascals, by the barometric formula for
/// the international standard atmosphere.
///
/// A weather service that reports a station pressure gives a better number
/// than this -- it knows what the weather is doing -- and should be preferred.
/// This is for when all that is known about a course is how high up it is,
/// which is the usual case for a course nobody has asked a service about.
///
/// Denver, a mile up, comes out near 83 kPa against 101 at the coast, and
/// that eighteen percent of air is most of why a ball carries further there.
[[nodiscard]] float pressure_at_elevation(
    float elevation_metres,
    float sea_level_pressure = standard_sea_level_pressure) noexcept;

[[nodiscard]] float air_density(
    float temperature_celsius,
    float pressure_pascals = standard_sea_level_pressure,
    float relative_humidity = 0.0F) noexcept;

} // namespace mgv::physics
