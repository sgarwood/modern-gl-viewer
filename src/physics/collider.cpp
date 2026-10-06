#include "mgv/physics/collider.hpp"

#include <stdexcept>
#include <utility>

namespace mgv::physics {

Collider::Collider(ColliderShape shape) : shape_{std::move(shape)} {}

Collider Collider::sphere(Length radius) {
    if (radius.metres() <= 0.0F) {
        throw std::invalid_argument{"Sphere collider radius must be positive"};
    }
    return Collider{SphereCollider{radius}};
}

Collider Collider::box(Dimensions half_extents) {
    const auto value = half_extents.metres();
    if (value.x <= 0.0F || value.y <= 0.0F || value.z <= 0.0F) {
        throw std::invalid_argument{"Box collider half extents must be positive"};
    }
    return Collider{BoxCollider{half_extents}};
}

Collider Collider::heightmap(int width, int depth, float scale_x, float scale_z, std::vector<float> heights) {
    if (width <= 1 || depth <= 1 || scale_x <= 0.0F || scale_z <= 0.0F || heights.size() != static_cast<size_t>(width * depth)) {
        throw std::invalid_argument{"Invalid heightmap dimensions"};
    }
    return Collider{HeightmapCollider{width, depth, scale_x, scale_z, std::move(heights)}};
}

const ColliderShape& Collider::shape() const noexcept { return shape_; }

} // namespace mgv::physics
