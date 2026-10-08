#include "mgv/transform.hpp"

#include <cmath>
#include <stdexcept>

namespace mgv {
namespace {

[[nodiscard]] float squared_length(const Vec3& value) {
    return value.x * value.x + value.y * value.y + value.z * value.z;
}

[[nodiscard]] Quaternion normalized(Quaternion value) {
    const auto magnitude = std::sqrt(
        value.x * value.x + value.y * value.y + value.z * value.z + value.w * value.w);
    if (magnitude <= 1.0e-6F) {
        throw std::invalid_argument{"Transform rotation quaternion must not be zero"};
    }
    return {value.x / magnitude, value.y / magnitude, value.z / magnitude, value.w / magnitude};
}

} // namespace

Quaternion Quaternion::from_axis_angle(Vec3 axis, float radians) {
    const auto axis_length_squared = squared_length(axis);
    if (axis_length_squared <= 1.0e-12F) {
        throw std::invalid_argument{"Rotation axis must not be zero"};
    }
    const auto inverse_length = 1.0F / std::sqrt(axis_length_squared);
    const auto half_angle = radians * 0.5F;
    const auto sine = std::sin(half_angle);
    return {
        axis.x * inverse_length * sine,
        axis.y * inverse_length * sine,
        axis.z * inverse_length * sine,
        std::cos(half_angle),
    };
}

Quaternion operator*(const Quaternion& lhs, const Quaternion& rhs) noexcept {
    return {
        lhs.w * rhs.x + lhs.x * rhs.w + lhs.y * rhs.z - lhs.z * rhs.y,
        lhs.w * rhs.y - lhs.x * rhs.z + lhs.y * rhs.w + lhs.z * rhs.x,
        lhs.w * rhs.z + lhs.x * rhs.y - lhs.y * rhs.x + lhs.z * rhs.w,
        lhs.w * rhs.w - lhs.x * rhs.x - lhs.y * rhs.y - lhs.z * rhs.z,
    };
}

Transform& Transform::set_position(Vec3 value) noexcept {
    position_ = value;
    return *this;
}

Transform& Transform::set_rotation(Quaternion value) {
    rotation_ = normalized(value);
    return *this;
}

Transform& Transform::set_scale(Vec3 value) noexcept {
    scale_ = value;
    return *this;
}

Transform& Transform::set_uniform_scale(float value) noexcept {
    return set_scale({value, value, value});
}

const Vec3& Transform::position() const noexcept { return position_; }
const Quaternion& Transform::rotation() const noexcept { return rotation_; }
const Vec3& Transform::scale() const noexcept { return scale_; }

Mat4 Transform::matrix() const noexcept {
    const auto x2 = rotation_.x + rotation_.x;
    const auto y2 = rotation_.y + rotation_.y;
    const auto z2 = rotation_.z + rotation_.z;
    const auto xx = rotation_.x * x2;
    const auto xy = rotation_.x * y2;
    const auto xz = rotation_.x * z2;
    const auto yy = rotation_.y * y2;
    const auto yz = rotation_.y * z2;
    const auto zz = rotation_.z * z2;
    const auto wx = rotation_.w * x2;
    const auto wy = rotation_.w * y2;
    const auto wz = rotation_.w * z2;

    return {
        (1.0F - yy - zz) * scale_.x,
        (xy + wz) * scale_.x,
        (xz - wy) * scale_.x,
        0.0F,
        (xy - wz) * scale_.y,
        (1.0F - xx - zz) * scale_.y,
        (yz + wx) * scale_.y,
        0.0F,
        (xz + wy) * scale_.z,
        (yz - wx) * scale_.z,
        (1.0F - xx - yy) * scale_.z,
        0.0F,
        position_.x,
        position_.y,
        position_.z,
        1.0F,
    };
}

} // namespace mgv
