#!/bin/bash
cat << 'HPP' > include/mgv/physics/collider.hpp
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
    [[nodiscard]] static Collider heightmap(int width, int depth, float scale_x, float scale_z, std::vector<float> heights);

    [[nodiscard]] const ColliderShape& shape() const noexcept;

private:
    explicit Collider(ColliderShape shape);

    ColliderShape shape_;
};

} // namespace mgv::physics
HPP

cat << 'CPP' > src/physics/collider.cpp
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
CPP

