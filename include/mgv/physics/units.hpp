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

class Impulse final {
public:
    explicit Impulse(Vec3 newton_seconds);
    [[nodiscard]] const Vec3& newton_seconds() const noexcept;

private:
    Vec3 newton_seconds_{};
};

} // namespace mgv::physics
