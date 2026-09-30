#pragma once

#include "mgv/material.hpp"
#include "mgv/mesh.hpp"
#include "mgv/transform.hpp"

#include <memory>
#include <span>
#include <vector>

namespace mgv {

class Renderable final {
public:
    Renderable(
        std::shared_ptr<const MeshData> mesh,
        std::shared_ptr<const Material> material,
        Transform transform = {});

    [[nodiscard]] const std::shared_ptr<const MeshData>& mesh() const noexcept;
    [[nodiscard]] const std::shared_ptr<const Material>& material() const noexcept;
    [[nodiscard]] const Transform& transform() const noexcept;
    [[nodiscard]] bool visible() const noexcept;

    Renderable& set_transform(Transform value) noexcept;
    Renderable& set_visible(bool value) noexcept;

private:
    std::shared_ptr<const MeshData> mesh_;
    std::shared_ptr<const Material> material_;
    Transform transform_;
    bool visible_{true};
};

class Scene final {
public:
    Renderable& add(Renderable renderable);
    [[nodiscard]] std::span<const Renderable> renderables() const noexcept;
    [[nodiscard]] bool empty() const noexcept;
    [[nodiscard]] std::size_t size() const noexcept;

private:
    std::vector<Renderable> renderables_;
};

} // namespace mgv
