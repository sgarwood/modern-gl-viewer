#pragma once

#include "mgv/physics/units.hpp"

#include <optional>
#include <variant>
#include <vector>

namespace mgv::physics {

struct SphereCollider final {
    Length radius;
};

struct BoxCollider final {
    Dimensions half_extents;
};

/// A cylinder with hemispherical ends, standing on the Y axis.
///
/// Upright rather than free: a trunk and a flagstick both stand up, and an
/// arbitrary axis would cost a rotation on every contact to serve nothing
/// this course contains. The caps are the price of the cheap closest-point
/// test, and on a tree they fall underground and up inside the canopy where
/// nothing can reach them.
struct CapsuleCollider final {
    Length radius;
    /// Half the length of the segment the radius is swept along, so the whole
    /// thing stands `2 * (half_height + radius)` tall.
    Length half_height;
};

struct HeightmapCollider final {
    int width;
    int depth;
    float scale_x;
    float scale_z;
    std::vector<float> heights;
};

using ColliderShape =
    std::variant<SphereCollider, BoxCollider, CapsuleCollider, HeightmapCollider>;

class Collider final {
public:
    [[nodiscard]] static Collider sphere(Length radius);
    [[nodiscard]] static Collider box(Dimensions half_extents);
    [[nodiscard]] static Collider capsule(Length radius, Length half_height);
    [[nodiscard]] static Collider heightmap(
        int width,
        int depth,
        float scale_x,
        float scale_z,
        std::vector<float> heights);

    [[nodiscard]] const ColliderShape& shape() const noexcept;

private:
    explicit Collider(ColliderShape shape);

    ColliderShape shape_;
};

/// The radius of a sphere collider, and nothing for any other shape.
///
/// A solver needs this in three places once bodies are allowed to spin --
/// the reference area for drag, the moment of inertia, and the lever arm
/// from a centre to a contact -- and all three have to agree on one number
/// or a ball rolls at a speed its own spin contradicts.
[[nodiscard]] std::optional<float> sphere_radius(const Collider& collider) noexcept;

} // namespace mgv::physics
