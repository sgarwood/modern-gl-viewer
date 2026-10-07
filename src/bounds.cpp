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

[[nodiscard]] Vec3 add(Vec3 lhs, Vec3 rhs) {
    return {lhs.x + rhs.x, lhs.y + rhs.y, lhs.z + rhs.z};
}

/// The smallest sphere containing both inputs.
[[nodiscard]] BoundingSphere merged(const BoundingSphere& lhs, const BoundingSphere& rhs) {
    const Vec3 between{
        rhs.center.x - lhs.center.x,
        rhs.center.y - lhs.center.y,
        rhs.center.z - lhs.center.z,
    };
    const auto separation = length(between);
    if (separation + rhs.radius <= lhs.radius) {
        return lhs;
    }
    if (separation + lhs.radius <= rhs.radius) {
        return rhs;
    }
    const auto radius = (separation + lhs.radius + rhs.radius) * 0.5F;
    const auto along = separation > 1.0e-6F ? (radius - lhs.radius) / separation : 0.0F;
    return {
        .center = {
            lhs.center.x + between.x * along,
            lhs.center.y + between.y * along,
            lhs.center.z + between.z * along,
        },
        .radius = radius,
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

    // The source geometry first, as a local box and a sphere about its centre.
    auto local_minimum = mesh.vertices.front().position;
    auto local_maximum = local_minimum;
    for (const auto& vertex : mesh.vertices) {
        local_minimum.x = std::min(local_minimum.x, vertex.position.x);
        local_minimum.y = std::min(local_minimum.y, vertex.position.y);
        local_minimum.z = std::min(local_minimum.z, vertex.position.z);
        local_maximum.x = std::max(local_maximum.x, vertex.position.x);
        local_maximum.y = std::max(local_maximum.y, vertex.position.y);
        local_maximum.z = std::max(local_maximum.z, vertex.position.z);
    }
    const Vec3 local_center{
        (local_minimum.x + local_maximum.x) * 0.5F,
        (local_minimum.y + local_maximum.y) * 0.5F,
        (local_minimum.z + local_maximum.z) * 0.5F,
    };
    float local_radius{};
    for (const auto& vertex : mesh.vertices) {
        local_radius = std::max(local_radius, length({
            vertex.position.x - local_center.x,
            vertex.position.y - local_center.y,
            vertex.position.z - local_center.z,
        }));
    }

    if (mesh.instances.empty()) {
        return {
            .box = {.minimum = local_minimum, .maximum = local_maximum},
            .sphere = {.center = local_center, .radius = local_radius},
        };
    }

    // With instances, the bounds have to cover every placement, or a field of
    // grass is culled on the strength of one blade sitting at the origin.
    //
    // Each placement's own sphere is the local sphere scaled and moved; yaw
    // only spins it about its centre, which a sphere is indifferent to. The
    // result is the smallest sphere enclosing all of them, grown from the
    // first rather than recentred, which is conservative and cheap.
    auto first = true;
    Vec3 minimum{};
    Vec3 maximum{};
    BoundingSphere sphere{};
    for (const auto& instance : mesh.instances) {
        const auto scale = std::max(instance.parameters.x, 0.0F);
        const auto sine = std::sin(instance.yaw);
        const auto cosine = std::cos(instance.yaw);
        const auto rotate = [sine, cosine](Vec3 point) {
            return Vec3{
                point.x * cosine + point.z * sine,
                point.y,
                -point.x * sine + point.z * cosine,
            };
        };

        const auto placed_center = add(
            rotate({local_center.x * scale, local_center.y * scale, local_center.z * scale}),
            instance.position);
        const auto placed_radius = local_radius * scale;

        // The rotated local box, bounded axis-aligned by its own corners.
        Vec3 corner_minimum{};
        Vec3 corner_maximum{};
        for (int index = 0; index < 8; ++index) {
            const Vec3 corner{
                (index & 1) != 0 ? local_maximum.x : local_minimum.x,
                (index & 2) != 0 ? local_maximum.y : local_minimum.y,
                (index & 4) != 0 ? local_maximum.z : local_minimum.z,
            };
            const auto placed = add(
                rotate({corner.x * scale, corner.y * scale, corner.z * scale}),
                instance.position);
            if (index == 0) {
                corner_minimum = placed;
                corner_maximum = placed;
                continue;
            }
            corner_minimum.x = std::min(corner_minimum.x, placed.x);
            corner_minimum.y = std::min(corner_minimum.y, placed.y);
            corner_minimum.z = std::min(corner_minimum.z, placed.z);
            corner_maximum.x = std::max(corner_maximum.x, placed.x);
            corner_maximum.y = std::max(corner_maximum.y, placed.y);
            corner_maximum.z = std::max(corner_maximum.z, placed.z);
        }

        if (first) {
            first = false;
            minimum = corner_minimum;
            maximum = corner_maximum;
            sphere = {.center = placed_center, .radius = placed_radius};
            continue;
        }
        minimum.x = std::min(minimum.x, corner_minimum.x);
        minimum.y = std::min(minimum.y, corner_minimum.y);
        minimum.z = std::min(minimum.z, corner_minimum.z);
        maximum.x = std::max(maximum.x, corner_maximum.x);
        maximum.y = std::max(maximum.y, corner_maximum.y);
        maximum.z = std::max(maximum.z, corner_maximum.z);
        sphere = merged(sphere, {.center = placed_center, .radius = placed_radius});
    }

    return {.box = {.minimum = minimum, .maximum = maximum}, .sphere = sphere};
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
