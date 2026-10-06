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

using ColliderShape = std::variant<SphereCollider, BoxCollider>;

class Collider final {
public:
    [[nodiscard]] static Collider sphere(Length radius);
    [[nodiscard]] static Collider box(Dimensions half_extents);

    [[nodiscard]] const ColliderShape& shape() const noexcept;

private:
    explicit Collider(ColliderShape shape);

    ColliderShape shape_;
};

} // namespace mgv::physics
