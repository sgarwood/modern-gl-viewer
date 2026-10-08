#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace mgv {

struct Vec2 final {
    float x{};
    float y{};

    friend bool operator==(const Vec2&, const Vec2&) = default;
};

struct Vec3 final {
    float x{};
    float y{};
    float z{};

    friend bool operator==(const Vec3&, const Vec3&) = default;
};

struct Vec4 final {
    float x{};
    float y{};
    float z{};
    float w{};

    friend bool operator==(const Vec4&, const Vec4&) = default;
};

struct Vertex final {
    Vec3 position;
    Vec3 normal;
    Vec2 tex_coord;

    friend bool operator==(const Vertex&, const Vertex&) = default;
};

/// One placement of a mesh, uploaded as a per-instance vertex attribute.
///
/// A blade of grass is a dozen vertices and there are hundreds of thousands
/// of them; the only thing that differs between them is where they stand and
/// how they grew. Sending that difference as instance data rather than as
/// geometry is the difference between a field of grass and a field of grass
/// the machine cannot hold.
struct MeshInstance final {
    /// World-space offset applied after the model matrix.
    Vec3 position{};
    /// Rotation about +Y, in radians.
    float yaw{};
    /// Free per-instance parameters. By convention x is a uniform scale and
    /// the rest are the mesh's own to interpret; the grass shader reads them
    /// as width, colour variation, and wind phase.
    Vec4 parameters{1.0F, 0.0F, 0.0F, 0.0F};

    friend bool operator==(const MeshInstance&, const MeshInstance&) = default;
};

/// How one vertex is attached to a skeleton.
///
/// Held in a stream of its own rather than widened into `Vertex`, because
/// almost nothing in a golf course is skinned. Terrain, grass, trees and
/// leaves would all pay twenty bytes a vertex for four joint indices and
/// four weights they never use.
struct SkinningVertex final {
    /// Indices into the skin's joint list. Four influences is what glTF's
    /// JOINTS_0 and every linear-blend implementation agree on.
    std::array<std::uint16_t, 4> joints{};
    /// Weights, which glTF requires to sum to one.
    std::array<float, 4> weights{1.0F, 0.0F, 0.0F, 0.0F};

    friend bool operator==(const SkinningVertex&, const SkinningVertex&) = default;
};

struct MeshData final {
    std::vector<Vertex> vertices;
    std::vector<std::uint32_t> indices;
    /// Placements of this mesh. Empty means a single placement at the origin,
    /// which is what every ordinary mesh is.
    std::vector<MeshInstance> instances{};
    /// Skinning influences, one per vertex, or empty for an unskinned mesh.
    std::vector<SkinningVertex> skinning{};

    [[nodiscard]] bool empty() const noexcept {
        return vertices.empty() || indices.empty();
    }

    /// Whether this mesh carries skinning influences for every vertex.
    [[nodiscard]] bool skinned() const noexcept {
        return !skinning.empty() && skinning.size() == vertices.size();
    }

    /// The number of placements actually drawn: never zero.
    [[nodiscard]] std::size_t instance_count() const noexcept {
        return instances.empty() ? 1 : instances.size();
    }
};

} // namespace mgv
