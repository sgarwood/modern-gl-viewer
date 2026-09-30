#pragma once

#include "mgv/camera.hpp"

namespace mgv {

struct Quaternion final {
    float x{};
    float y{};
    float z{};
    float w{1.0F};

    [[nodiscard]] static Quaternion from_axis_angle(Vec3 axis, float radians);
    friend bool operator==(const Quaternion&, const Quaternion&) = default;
};

class Transform final {
public:
    Transform& set_position(Vec3 value) noexcept;
    Transform& set_rotation(Quaternion value);
    Transform& set_scale(Vec3 value) noexcept;
    Transform& set_uniform_scale(float value) noexcept;

    [[nodiscard]] const Vec3& position() const noexcept;
    [[nodiscard]] const Quaternion& rotation() const noexcept;
    [[nodiscard]] const Vec3& scale() const noexcept;
    [[nodiscard]] Mat4 matrix() const noexcept;

private:
    Vec3 position_{};
    Quaternion rotation_{};
    Vec3 scale_{1.0F, 1.0F, 1.0F};
};

} // namespace mgv
