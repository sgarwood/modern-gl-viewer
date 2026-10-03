#pragma once

#include "mgv/camera.hpp"
#include "mgv/clip_space.hpp"
#include "mgv/mesh.hpp"

#include <array>

namespace mgv {

struct AxisAlignedBoundingBox final {
    Vec3 minimum;
    Vec3 maximum;

    friend bool operator==(const AxisAlignedBoundingBox&, const AxisAlignedBoundingBox&) = default;
};

struct BoundingSphere final {
    Vec3 center;
    float radius{};

    friend bool operator==(const BoundingSphere&, const BoundingSphere&) = default;
};

struct MeshBounds final {
    AxisAlignedBoundingBox box;
    BoundingSphere sphere;
};

struct Plane final {
    Vec3 normal;
    float distance{};
};

class Frustum final {
public:
    [[nodiscard]] static Frustum from_view_projection(
        const Mat4& view_projection,
        ClipSpaceConvention convention = {});

    [[nodiscard]] bool intersects(const BoundingSphere& sphere) const noexcept;

private:
    explicit Frustum(std::array<Plane, 6> planes) noexcept;

    std::array<Plane, 6> planes_;
};

[[nodiscard]] MeshBounds calculate_bounds(const MeshData& mesh);
[[nodiscard]] BoundingSphere transform_bounds(const BoundingSphere& sphere, const Mat4& transform);

} // namespace mgv
