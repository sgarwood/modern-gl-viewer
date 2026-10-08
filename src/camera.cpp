#include "mgv/camera.hpp"

#include <cmath>
#include <numbers>
#include <stdexcept>

namespace mgv {
namespace {

[[nodiscard]] Vec3 subtract(const Vec3& lhs, const Vec3& rhs) {
    return {lhs.x - rhs.x, lhs.y - rhs.y, lhs.z - rhs.z};
}

[[nodiscard]] float dot(const Vec3& lhs, const Vec3& rhs) {
    return lhs.x * rhs.x + lhs.y * rhs.y + lhs.z * rhs.z;
}

[[nodiscard]] Vec3 cross(const Vec3& lhs, const Vec3& rhs) {
    return {
        lhs.y * rhs.z - lhs.z * rhs.y,
        lhs.z * rhs.x - lhs.x * rhs.z,
        lhs.x * rhs.y - lhs.y * rhs.x,
    };
}

[[nodiscard]] float length(const Vec3& value) {
    return std::sqrt(dot(value, value));
}

[[nodiscard]] Vec3 normalized(const Vec3& value, const char* error) {
    const auto magnitude = length(value);
    if (magnitude <= 1.0e-6F) {
        throw std::invalid_argument{error};
    }
    return {value.x / magnitude, value.y / magnitude, value.z / magnitude};
}

} // namespace

Mat4 multiply(const Mat4& lhs, const Mat4& rhs) noexcept {
    Mat4 result{};
    for (std::size_t column = 0; column < 4; ++column) {
        for (std::size_t row = 0; row < 4; ++row) {
            for (std::size_t inner = 0; inner < 4; ++inner) {
                result[column * 4 + row] += lhs[inner * 4 + row] * rhs[column * 4 + inner];
            }
        }
    }
    return result;
}

void Camera::look_at(Vec3 position, Vec3 target, Vec3 up) {
    const auto forward = normalized(subtract(target, position), "Camera position and target must differ");
    static_cast<void>(normalized(cross(forward, up), "Camera up direction must not be parallel to its view"));
    position_ = position;
    target_ = target;
    up_ = normalized(up, "Camera up direction must not be zero");
}

void Camera::set_perspective(float vertical_field_of_view_degrees, float near_plane, float far_plane) {
    if (vertical_field_of_view_degrees <= 0.0F || vertical_field_of_view_degrees >= 180.0F) {
        throw std::invalid_argument{"Camera field of view must be between 0 and 180 degrees"};
    }
    if (near_plane <= 0.0F || far_plane <= near_plane) {
        throw std::invalid_argument{"Camera clipping planes must satisfy 0 < near < far"};
    }
    vertical_field_of_view_degrees_ = vertical_field_of_view_degrees;
    near_plane_ = near_plane;
    far_plane_ = far_plane;
}

const Vec3& Camera::position() const noexcept { return position_; }
const Vec3& Camera::target() const noexcept { return target_; }
const Vec3& Camera::up() const noexcept { return up_; }
float Camera::vertical_field_of_view_degrees() const noexcept {
    return vertical_field_of_view_degrees_;
}
float Camera::near_plane() const noexcept { return near_plane_; }
float Camera::far_plane() const noexcept { return far_plane_; }

Mat4 Camera::view_matrix() const {
    const auto forward = normalized(subtract(target_, position_), "Camera position and target must differ");
    const auto side = normalized(cross(forward, up_), "Camera up direction must not be parallel to its view");
    const auto corrected_up = cross(side, forward);

    return {
        side.x, corrected_up.x, -forward.x, 0.0F,
        side.y, corrected_up.y, -forward.y, 0.0F,
        side.z, corrected_up.z, -forward.z, 0.0F,
        -dot(side, position_), -dot(corrected_up, position_), dot(forward, position_), 1.0F,
    };
}

Mat4 Camera::projection_matrix(float aspect_ratio, ClipSpaceConvention convention) const {
    if (aspect_ratio <= 0.0F) {
        throw std::invalid_argument{"Camera aspect ratio must be positive"};
    }
    const auto radians = vertical_field_of_view_degrees_ * std::numbers::pi_v<float> / 180.0F;
    const auto focal_length = 1.0F / std::tan(radians * 0.5F);
    Mat4 result{};
    result[0] = focal_length / aspect_ratio;
    result[5] = convention.invert_y ? -focal_length : focal_length;
    if (convention.depth_range == ClipDepthRange::zero_to_one) {
        result[10] = far_plane_ / (near_plane_ - far_plane_);
        result[14] = (far_plane_ * near_plane_) / (near_plane_ - far_plane_);
    } else {
        result[10] = (far_plane_ + near_plane_) / (near_plane_ - far_plane_);
        result[14] = (2.0F * far_plane_ * near_plane_) / (near_plane_ - far_plane_);
    }
    result[11] = -1.0F;
    return result;
}

Mat4 Camera::view_projection_matrix(float aspect_ratio, ClipSpaceConvention convention) const {
    return multiply(projection_matrix(aspect_ratio, convention), view_matrix());
}

} // namespace mgv
