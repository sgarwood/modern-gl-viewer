#pragma once

#include "mgv/camera.hpp"

namespace mgv {

struct Quaternion final {
    float x{};
    float y{};
    float z{};
    float w{1.0F};

    [[nodiscard]] static Quaternion from_axis_angle(Vec3 axis, float radians);

    /// Composition: `a * b` applies `b` first, then `a`, matching how the
    /// equivalent matrices would multiply.
    friend Quaternion operator*(const Quaternion& lhs, const Quaternion& rhs) noexcept;

    friend bool operator==(const Quaternion&, const Quaternion&) = default;
};

class Transform final {
public:
    /// Recovers a transform from a matrix built of a rotation, a uniform
    /// scale and a translation.
    ///
    /// Anything socketed to a skeleton arrives as a matrix, because that is
    /// what a joint palette is made of, and the scene wants position,
    /// rotation and scale. The decomposition assumes the matrix really is
    /// rigid-plus-uniform-scale: a joint matrix from an animation is, but a
    /// matrix carrying shear or non-uniform scale will not survive the
    /// round trip, and nothing here will tell you so.
    [[nodiscard]] static Transform from_matrix(const Mat4& matrix);

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
