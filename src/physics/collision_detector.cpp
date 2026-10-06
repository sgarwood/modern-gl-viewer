#include "mgv/physics/collision_detector.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
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
    const auto local_pos = subtract(sphere_position, heightmap_position);
    const auto grid_x = local_pos.x / heightmap.scale_x;
    const auto grid_z = local_pos.z / heightmap.scale_z;
    const auto maximum_x = static_cast<float>(heightmap.width - 1);
    const auto maximum_z = static_cast<float>(heightmap.depth - 1);
    if (grid_x < 0.0F || grid_x >= maximum_x ||
        grid_z < 0.0F || grid_z >= maximum_z) {
        return std::nullopt;
    }

    const auto x0 = static_cast<int>(grid_x);
    const auto z0 = static_cast<int>(grid_z);
    const auto interpolation_x = grid_x - static_cast<float>(x0);
    const auto interpolation_z = grid_z - static_cast<float>(z0);
    const auto sample = [&heightmap](int x, int z) {
        const auto index = static_cast<std::size_t>(z) *
                               static_cast<std::size_t>(heightmap.width) +
                           static_cast<std::size_t>(x);
        return heightmap.heights[index];
    };
    const auto height00 = sample(x0, z0);
    const auto height10 = sample(x0 + 1, z0);
    const auto height01 = sample(x0, z0 + 1);
    const auto height11 = sample(x0 + 1, z0 + 1);

    const auto near_height =
        height00 * (1.0F - interpolation_x) + height10 * interpolation_x;
    const auto far_height =
        height01 * (1.0F - interpolation_x) + height11 * interpolation_x;
    const auto height_at_position =
        near_height * (1.0F - interpolation_z) + far_height * interpolation_z;

    if (local_pos.y > height_at_position + sphere.radius.metres()) {
        return std::nullopt;
    }

    const Vec3 v1{heightmap.scale_x, height10 - height00, 0.0F};
    const Vec3 v2{0.0F, height01 - height00, heightmap.scale_z};
    Vec3 normal = {
        v1.y * v2.z - v1.z * v2.y,
        v1.z * v2.x - v1.x * v2.z,
        v1.x * v2.y - v1.y * v2.x,
    };

    const auto magnitude = std::sqrt(dot(normal, normal));
    if (magnitude > 0.0F) {
        normal = scaled(normal, 1.0F / magnitude);
    } else {
        normal = {0.0F, 1.0F, 0.0F};
    }

    const auto penetration =
        height_at_position + sphere.radius.metres() - local_pos.y;
    if (penetration > 0.0F) {
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

} // namespace mgv::physics
