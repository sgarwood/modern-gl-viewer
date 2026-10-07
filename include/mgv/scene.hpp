#pragma once

#include "mgv/material.hpp"
#include "mgv/mesh.hpp"
#include "mgv/transform.hpp"

#include <cstdint>
#include <memory>
#include <span>
#include <vector>

namespace mgv {

struct RenderableId final {
    std::uint64_t value{};

    friend bool operator==(const RenderableId&, const RenderableId&) = default;
};

class Renderable final {
public:
    Renderable(
        std::shared_ptr<const MeshData> mesh,
        std::shared_ptr<const MaterialInstance> material,
        Transform transform = {});

    [[nodiscard]] const std::shared_ptr<const MeshData>& mesh() const noexcept;
    [[nodiscard]] const std::shared_ptr<const MaterialInstance>& material_instance() const noexcept;
    [[nodiscard]] const Transform& transform() const noexcept;
    [[nodiscard]] bool visible() const noexcept;
    /// Whether this renderable is drawn into the sun's shadow map. A sky dome
    /// surrounds the camera, so it would shadow the entire world.
    [[nodiscard]] bool casts_shadow() const noexcept;

    Renderable& set_transform(Transform value) noexcept;
    Renderable& set_visible(bool value) noexcept;
    Renderable& set_casts_shadow(bool value) noexcept;

private:
    std::shared_ptr<const MeshData> mesh_;
    std::shared_ptr<const MaterialInstance> material_;
    Transform transform_;
    bool visible_{true};
    bool casts_shadow_{true};
};

class Scene final {
public:
    RenderableId add(Renderable renderable);
    [[nodiscard]] bool remove(RenderableId id);
    [[nodiscard]] bool contains(RenderableId id) const noexcept;
    [[nodiscard]] Renderable& renderable(RenderableId id);
    [[nodiscard]] const Renderable& renderable(RenderableId id) const;
    [[nodiscard]] std::span<const Renderable> renderables() const noexcept;
    [[nodiscard]] std::span<const RenderableId> renderable_ids() const noexcept;
    [[nodiscard]] bool empty() const noexcept;
    [[nodiscard]] std::size_t size() const noexcept;

private:
    std::vector<Renderable> renderables_;
    std::vector<RenderableId> renderable_ids_;
};

} // namespace mgv
