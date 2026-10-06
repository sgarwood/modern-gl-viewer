#include "mgv/physics/collision_detector.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <optional>
#include <stdexcept>
#include <variant>

namespace mgv::physics {
namespace {

struct Manifold final {
    Vec3 normal;
    float penetration{};
};

[[nodiscard]] Vec3 subtract(Vec3 lhs, Vec3 rhs) {
    return {lhs.x - rhs.x, lhs.y - rhs.y, lhs.z - rhs.z};
}

[[nodiscard]] float dot(Vec3 lhs, Vec3 rhs) {
    return lhs.x * rhs.x + lhs.y * rhs.y + lhs.z * rhs.z;
}

[[nodiscard]] Vec3 scaled(Vec3 value, float scale) {
    return {value.x * scale, value.y * scale, value.z * scale};
}

[[nodiscard]] Vec3 negate(Vec3 value) {
    return {-value.x, -value.y, -value.z};
}

[[nodiscard]] std::optional<Manifold> sphere_sphere(
    const SphereCollider& first,
    Vec3 first_position,
    const SphereCollider& second,
    Vec3 second_position) {
    const auto delta = subtract(second_position, first_position);
    const auto distance_squared = dot(delta, delta);
    const auto combined_radius = first.radius.metres() + second.radius.metres();
    if (distance_squared > combined_radius * combined_radius) {
        return std::nullopt;
    }
    const auto distance = std::sqrt(distance_squared);
    const auto normal = distance > std::numeric_limits<float>::epsilon()
        ? scaled(delta, 1.0F / distance)
        : Vec3{1.0F, 0.0F, 0.0F};
    return Manifold{.normal = normal, .penetration = combined_radius - distance};
}

[[nodiscard]] std::optional<Manifold> box_box(
    const BoxCollider& first,
    Vec3 first_position,
    const BoxCollider& second,
    Vec3 second_position) {
    const auto delta = subtract(second_position, first_position);
    const auto first_half = first.half_extents.metres();
    const auto second_half = second.half_extents.metres();
    const Vec3 overlap{
        first_half.x + second_half.x - std::abs(delta.x),
        first_half.y + second_half.y - std::abs(delta.y),
        first_half.z + second_half.z - std::abs(delta.z),
    };
    if (overlap.x < 0.0F || overlap.y < 0.0F || overlap.z < 0.0F) {
        return std::nullopt;
    }
    if (overlap.x <= overlap.y && overlap.x <= overlap.z) {
        return Manifold{
            .normal = {delta.x >= 0.0F ? 1.0F : -1.0F, 0.0F, 0.0F},
            .penetration = overlap.x,
        };
    }
    if (overlap.y <= overlap.z) {
        return Manifold{
            .normal = {0.0F, delta.y >= 0.0F ? 1.0F : -1.0F, 0.0F},
            .penetration = overlap.y,
        };
    }
    return Manifold{
        .normal = {0.0F, 0.0F, delta.z >= 0.0F ? 1.0F : -1.0F},
        .penetration = overlap.z,
    };
}

[[nodiscard]] std::optional<Manifold> sphere_box(
    const SphereCollider& sphere,
    Vec3 sphere_position,
    const BoxCollider& box,
    Vec3 box_position) {
    const auto half = box.half_extents.metres();
    const auto local = subtract(sphere_position, box_position);
    const auto inside = std::abs(local.x) <= half.x &&
                        std::abs(local.y) <= half.y &&
                        std::abs(local.z) <= half.z;
    if (inside) {
        const Vec3 face_distance{
            half.x - std::abs(local.x),
            half.y - std::abs(local.y),
            half.z - std::abs(local.z),
        };
        if (face_distance.x <= face_distance.y && face_distance.x <= face_distance.z) {
            return Manifold{
                .normal = {local.x >= 0.0F ? -1.0F : 1.0F, 0.0F, 0.0F},
                .penetration = sphere.radius.metres() + face_distance.x,
            };
        }
        if (face_distance.y <= face_distance.z) {
            return Manifold{
                .normal = {0.0F, local.y >= 0.0F ? -1.0F : 1.0F, 0.0F},
                .penetration = sphere.radius.metres() + face_distance.y,
            };
        }
        return Manifold{
            .normal = {0.0F, 0.0F, local.z >= 0.0F ? -1.0F : 1.0F},
            .penetration = sphere.radius.metres() + face_distance.z,
        };
    }

    const Vec3 closest_local{
        std::clamp(local.x, -half.x, half.x),
        std::clamp(local.y, -half.y, half.y),
        std::clamp(local.z, -half.z, half.z),
    };
    const auto closest = Vec3{
        box_position.x + closest_local.x,
        box_position.y + closest_local.y,
        box_position.z + closest_local.z,
    };
    const auto delta = subtract(closest, sphere_position);
    const auto distance_squared = dot(delta, delta);
    const auto radius = sphere.radius.metres();
    if (distance_squared > radius * radius) {
        return std::nullopt;
    }
    const auto distance = std::sqrt(distance_squared);
    const auto normal = distance > std::numeric_limits<float>::epsilon()
        ? scaled(delta, 1.0F / distance)
        : Vec3{1.0F, 0.0F, 0.0F};
    return Manifold{.normal = normal, .penetration = radius - distance};
}


[[nodiscard]] std::optional<Manifold> sphere_heightmap(
    const SphereCollider& sphere,
    Vec3 sphere_position,
    const HeightmapCollider& heightmap,
    Vec3 heightmap_position) {
    
    // Transform sphere into heightmap local space
    Vec3 local_pos = subtract(sphere_position, heightmap_position);
    
    // Map to grid coordinates
    float gx = local_pos.x / heightmap.scale_x;
    float gz = local_pos.z / heightmap.scale_z;
    
    // Check bounds
    if (gx < 0 || gx >= heightmap.width - 1 || gz < 0 || gz >= heightmap.depth - 1) {
        return std::nullopt;
    }
    
    int x0 = static_cast<int>(gx);
    int z0 = static_cast<int>(gz);
    float tx = gx - x0;
    float tz = gz - z0;
    
    // Get heights of the 4 corners of the quad
    float h00 = heightmap.heights[z0 * heightmap.width + x0];
    float h10 = heightmap.heights[z0 * heightmap.width + (x0 + 1)];
    float h01 = heightmap.heights[(z0 + 1) * heightmap.width + x0];
    float h11 = heightmap.heights[(z0 + 1) * heightmap.width + (x0 + 1)];
    
    // Bilinear interpolation for height
    float hy0 = h00 * (1 - tx) + h10 * tx;
    float hy1 = h01 * (1 - tx) + h11 * tx;
    float height_at_pos = hy0 * (1 - tz) + hy1 * tz;
    
    // If the sphere is above the ground by more than its radius, no collision
    if (local_pos.y > height_at_pos + sphere.radius.metres()) {
        return std::nullopt;
    }
    
    // Calculate normal using cross product of diagonals
    Vec3 v1 = {heightmap.scale_x, h10 - h00, 0.0f};
    Vec3 v2 = {0.0f, h01 - h00, heightmap.scale_z};
    Vec3 normal = {
        v1.y * v2.z - v1.z * v2.y,
        v1.z * v2.x - v1.x * v2.z,
        v1.x * v2.y - v1.y * v2.x
    };
    
    // Normalize
    float mag = std::sqrt(dot(normal, normal));
    if (mag > 0) {
        normal = scaled(normal, 1.0f / mag);
    } else {
        normal = {0.0f, 1.0f, 0.0f};
    }
    
    // Calculate penetration distance
    float penetration = (height_at_pos + sphere.radius.metres()) - local_pos.y;
    
    if (penetration > 0) {
        return Manifold{.normal = normal, .penetration = penetration};
    }
    return std::nullopt;
}

} // namespace

ContactManifold::ContactManifold(Vec3 normal, Length penetration)
    : normal_{normal}, penetration_{penetration} {
    const auto magnitude_squared = dot(normal_, normal_);
    if (!std::isfinite(magnitude_squared) ||
        magnitude_squared <= std::numeric_limits<float>::epsilon()) {
        throw std::invalid_argument{"Contact normal must be finite and non-zero"};
    }
    normal_ = scaled(normal_, 1.0F / std::sqrt(magnitude_squared));
}

const Vec3& ContactManifold::normal() const noexcept { return normal_; }
const Length& ContactManifold::penetration() const noexcept { return penetration_; }

std::optional<ContactManifold> DiscreteCollisionDetector::detect(
    const RigidBody& first,
    const RigidBody& second) const {
    const auto& first_shape = first.collider().shape();
    const auto& second_shape = second.collider().shape();
    const auto first_position = first.position().metres();
    const auto second_position = second.position().metres();
    std::optional<Manifold> manifold;

    if (const auto* first_sphere = std::get_if<SphereCollider>(&first_shape)) {
        if (const auto* second_sphere = std::get_if<SphereCollider>(&second_shape)) {
            manifold = sphere_sphere(*first_sphere, first_position, *second_sphere, second_position);
        } else if (const auto* second_box = std::get_if<BoxCollider>(&second_shape)) {
            manifold = sphere_box(*first_sphere, first_position, *second_box, second_position);
        } else if (const auto* second_hm = std::get_if<HeightmapCollider>(&second_shape)) {
            manifold = sphere_heightmap(*first_sphere, first_position, *second_hm, second_position);
        }
    } else if (const auto* first_box = std::get_if<BoxCollider>(&first_shape)) {
        if (const auto* second_sphere = std::get_if<SphereCollider>(&second_shape)) {
            manifold = sphere_box(*second_sphere, second_position, *first_box, first_position);
            if (manifold) manifold->normal = negate(manifold->normal);
        } else if (const auto* second_box = std::get_if<BoxCollider>(&second_shape)) {
            manifold = box_box(*first_box, first_position, *second_box, second_position);
        }
    } else if (const auto* first_hm = std::get_if<HeightmapCollider>(&first_shape)) {
        if (const auto* second_sphere = std::get_if<SphereCollider>(&second_shape)) {
            manifold = sphere_heightmap(*second_sphere, second_position, *first_hm, first_position);
            if (manifold) manifold->normal = negate(manifold->normal);
        }
    }

    if (!manifold) {
        return std::nullopt;
    }
    return ContactManifold{manifold->normal, Length{manifold->penetration}};
}


[[nodiscard]] std::optional<Manifold> sphere_heightmap(
    const SphereCollider& sphere,
    Vec3 sphere_position,
    const HeightmapCollider& heightmap,
    Vec3 heightmap_position) {
    
    // Transform sphere into heightmap local space
    Vec3 local_pos = subtract(sphere_position, heightmap_position);
    
    // Map to grid coordinates
    float gx = local_pos.x / heightmap.scale_x;
    float gz = local_pos.z / heightmap.scale_z;
    
    // Check bounds
    if (gx < 0 || gx >= heightmap.width - 1 || gz < 0 || gz >= heightmap.depth - 1) {
        return std::nullopt;
    }
    
    int x0 = static_cast<int>(gx);
    int z0 = static_cast<int>(gz);
    float tx = gx - x0;
    float tz = gz - z0;
    
    // Get heights of the 4 corners of the quad
    float h00 = heightmap.heights[z0 * heightmap.width + x0];
    float h10 = heightmap.heights[z0 * heightmap.width + (x0 + 1)];
    float h01 = heightmap.heights[(z0 + 1) * heightmap.width + x0];
    float h11 = heightmap.heights[(z0 + 1) * heightmap.width + (x0 + 1)];
    
    // Bilinear interpolation for height
    float hy0 = h00 * (1 - tx) + h10 * tx;
    float hy1 = h01 * (1 - tx) + h11 * tx;
    float height_at_pos = hy0 * (1 - tz) + hy1 * tz;
    
    // If the sphere is above the ground by more than its radius, no collision
    if (local_pos.y > height_at_pos + sphere.radius.metres()) {
        return std::nullopt;
    }
    
    // Calculate normal using cross product of diagonals
    Vec3 v1 = {heightmap.scale_x, h10 - h00, 0.0f};
    Vec3 v2 = {0.0f, h01 - h00, heightmap.scale_z};
    Vec3 normal = {
        v1.y * v2.z - v1.z * v2.y,
        v1.z * v2.x - v1.x * v2.z,
        v1.x * v2.y - v1.y * v2.x
    };
    
    // Normalize
    float mag = std::sqrt(dot(normal, normal));
    if (mag > 0) {
        normal = scaled(normal, 1.0f / mag);
    } else {
        normal = {0.0f, 1.0f, 0.0f};
    }
    
    // Calculate penetration distance
    float penetration = (height_at_pos + sphere.radius.metres()) - local_pos.y;
    
    if (penetration > 0) {
        return Manifold{.normal = normal, .penetration = penetration};
    }
    return std::nullopt;
}

} // namespace mgv::physics
