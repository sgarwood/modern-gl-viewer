#pragma once

#include "mgv/clip_space.hpp"
#include "mgv/mesh.hpp"

#include <array>

namespace mgv {

using Mat4 = std::array<float, 16>;

class Camera final {
public:
    Camera() = default;

    void look_at(Vec3 position, Vec3 target, Vec3 up = {0.0F, 1.0F, 0.0F});
    void set_perspective(float vertical_field_of_view_degrees, float near_plane, float far_plane);

    [[nodiscard]] const Vec3& position() const noexcept;
    [[nodiscard]] const Vec3& target() const noexcept;
    [[nodiscard]] const Vec3& up() const noexcept;
    [[nodiscard]] float vertical_field_of_view_degrees() const noexcept;
    [[nodiscard]] float near_plane() const noexcept;
    [[nodiscard]] float far_plane() const noexcept;
    [[nodiscard]] Mat4 view_matrix() const;
    [[nodiscard]] Mat4 projection_matrix(
        float aspect_ratio,
        ClipSpaceConvention convention = {}) const;
    [[nodiscard]] Mat4 view_projection_matrix(
        float aspect_ratio,
        ClipSpaceConvention convention = {}) const;

private:
    Vec3 position_{0.0F, 0.0F, 3.0F};
    Vec3 target_{};
    Vec3 up_{0.0F, 1.0F, 0.0F};
    float vertical_field_of_view_degrees_{45.0F};
    float near_plane_{0.1F};
    float far_plane_{100.0F};
};

} // namespace mgv
