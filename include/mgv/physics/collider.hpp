#pragma once

#include "mgv/physics/units.hpp"

#include <variant>
#include <vector>

namespace mgv::physics {

struct SphereCollider final {
    Length radius;
};

struct BoxCollider final {
    Dimensions half_extents;
};

struct HeightmapCollider final {
    int width;
    int depth;
    float scale_x;
    float scale_z;
    std::vector<float> heights;
};

using ColliderShape = std::variant<SphereCollider, BoxCollider, HeightmapCollider>;

class Collider final {
public:
    [[nodiscard]] static Collider sphere(Length radius);
    [[nodiscard]] static Collider box(Dimensions half_extents);
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

} // namespace mgv::physics
