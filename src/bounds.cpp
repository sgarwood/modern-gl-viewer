#include "mgv/bounds.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>
#include <utility>

namespace mgv {
namespace {

[[nodiscard]] float length(Vec3 value) {
    return std::sqrt(value.x * value.x + value.y * value.y + value.z * value.z);
}

[[nodiscard]] Plane normalized_plane(float x, float y, float z, float distance) {
    const auto magnitude = length({x, y, z});
    if (magnitude <= std::numeric_limits<float>::epsilon()) {
        throw std::invalid_argument{"Cannot extract a frustum from a degenerate matrix"};
    }
    return {
        .normal = {x / magnitude, y / magnitude, z / magnitude},
        .distance = distance / magnitude,
    };
}

[[nodiscard]] Vec3 transform_point(const Mat4& matrix, Vec3 point) {
    return {
        matrix[0] * point.x + matrix[4] * point.y + matrix[8] * point.z + matrix[12],
        matrix[1] * point.x + matrix[5] * point.y + matrix[9] * point.z + matrix[13],
        matrix[2] * point.x + matrix[6] * point.y + matrix[10] * point.z + matrix[14],
    };
}

[[nodiscard]] float distance_to_plane(const Plane& plane, Vec3 point) {
    return plane.normal.x * point.x + plane.normal.y * point.y +
           plane.normal.z * point.z + plane.distance;
}

} // namespace

Frustum::Frustum(std::array<Plane, 6> planes) noexcept : planes_{std::move(planes)} {}

Frustum Frustum::from_view_projection(
    const Mat4& matrix,
    ClipSpaceConvention convention) {
    const auto plane = [&matrix](std::size_t first_row, float first_scale,
                                 std::size_t second_row, float second_scale) {
        return normalized_plane(
            first_scale * matrix[first_row] + second_scale * matrix[second_row],
            first_scale * matrix[4 + first_row] + second_scale * matrix[4 + second_row],
            first_scale * matrix[8 + first_row] + second_scale * matrix[8 + second_row],
            first_scale * matrix[12 + first_row] + second_scale * matrix[12 + second_row]);
    };
    const auto near_plane = convention.depth_range == ClipDepthRange::zero_to_one
        ? normalized_plane(matrix[2], matrix[6], matrix[10], matrix[14])
        : plane(3, 1.0F, 2, 1.0F);
    return Frustum{{
        plane(3, 1.0F, 0, 1.0F),
        plane(3, 1.0F, 0, -1.0F),
        plane(3, 1.0F, 1, 1.0F),
        plane(3, 1.0F, 1, -1.0F),
        near_plane,
        plane(3, 1.0F, 2, -1.0F),
    }};
}

bool Frustum::intersects(const BoundingSphere& sphere) const noexcept {
    return std::ranges::all_of(planes_, [&sphere](const Plane& plane) {
        return distance_to_plane(plane, sphere.center) >= -sphere.radius;
    });
}

MeshBounds calculate_bounds(const MeshData& mesh) {
    if (mesh.empty()) {
        throw std::invalid_argument{"Cannot calculate bounds for an empty mesh"};
    }
    auto minimum = mesh.vertices.front().position;
    auto maximum = minimum;
    for (const auto& vertex : mesh.vertices) {
        minimum.x = std::min(minimum.x, vertex.position.x);
        minimum.y = std::min(minimum.y, vertex.position.y);
        minimum.z = std::min(minimum.z, vertex.position.z);
        maximum.x = std::max(maximum.x, vertex.position.x);
        maximum.y = std::max(maximum.y, vertex.position.y);
        maximum.z = std::max(maximum.z, vertex.position.z);
    }
    const Vec3 center{
        (minimum.x + maximum.x) * 0.5F,
        (minimum.y + maximum.y) * 0.5F,
        (minimum.z + maximum.z) * 0.5F,
    };
    float radius{};
    for (const auto& vertex : mesh.vertices) {
        const Vec3 offset{
            vertex.position.x - center.x,
            vertex.position.y - center.y,
            vertex.position.z - center.z,
        };
        radius = std::max(radius, length(offset));
    }
    return {
        .box = {.minimum = minimum, .maximum = maximum},
        .sphere = {.center = center, .radius = radius},
    };
}

BoundingSphere transform_bounds(const BoundingSphere& sphere, const Mat4& transform) {
    const auto scale_x = length({transform[0], transform[1], transform[2]});
    const auto scale_y = length({transform[4], transform[5], transform[6]});
    const auto scale_z = length({transform[8], transform[9], transform[10]});
    return {
        .center = transform_point(transform, sphere.center),
        .radius = sphere.radius * std::max({scale_x, scale_y, scale_z}),
    };
}

} // namespace mgv
