#pragma once

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

struct Vertex final {
    Vec3 position;
    Vec3 normal;
    Vec2 tex_coord;

    friend bool operator==(const Vertex&, const Vertex&) = default;
};

struct MeshData final {
    std::vector<Vertex> vertices;
    std::vector<std::uint32_t> indices;

    [[nodiscard]] bool empty() const noexcept {
        return vertices.empty() || indices.empty();
    }
};

} // namespace mgv
