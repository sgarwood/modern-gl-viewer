#include "mgv/scene.hpp"

#include <algorithm>
#include <atomic>
#include <limits>
#include <stdexcept>
#include <utility>

namespace mgv {
namespace {

std::atomic<std::uint64_t> next_renderable_id{1};

[[nodiscard]] RenderableId allocate_renderable_id() {
    auto value = next_renderable_id.load(std::memory_order_relaxed);
    while (value != std::numeric_limits<std::uint64_t>::max()) {
        if (next_renderable_id.compare_exchange_weak(
                value,
                value + 1,
                std::memory_order_relaxed,
                std::memory_order_relaxed)) {
            return RenderableId{value};
        }
    }
    throw std::overflow_error{"Renderable identifier capacity exhausted"};
}

} // namespace

Renderable::Renderable(
    std::shared_ptr<const MeshData> mesh,
    std::shared_ptr<const MaterialInstance> material,
    Transform transform)
    : mesh_{std::move(mesh)}, material_{std::move(material)}, transform_{std::move(transform)} {
    if (!mesh_ || mesh_->empty()) {
        throw std::invalid_argument{"Renderable requires a non-empty mesh"};
    }
    if (!material_) {
        throw std::invalid_argument{"Renderable requires a material"};
    }
}

const std::shared_ptr<const MeshData>& Renderable::mesh() const noexcept { return mesh_; }
const std::shared_ptr<const MaterialInstance>& Renderable::material_instance() const noexcept {
    return material_;
}
const Transform& Renderable::transform() const noexcept { return transform_; }
bool Renderable::visible() const noexcept { return visible_; }

Renderable& Renderable::set_transform(Transform value) noexcept {
    transform_ = std::move(value);
    return *this;
}

Renderable& Renderable::set_visible(bool value) noexcept {
    visible_ = value;
    return *this;
}

RenderableId Scene::add(Renderable renderable) {
    const auto id = allocate_renderable_id();
    renderables_.push_back(std::move(renderable));
    try {
        renderable_ids_.push_back(id);
    } catch (...) {
        renderables_.pop_back();
        throw;
    }
    return id;
}

bool Scene::remove(RenderableId id) {
    const auto found = std::ranges::find(renderable_ids_, id);
    if (found == renderable_ids_.end()) {
        return false;
    }
    const auto index = static_cast<std::size_t>(std::distance(renderable_ids_.begin(), found));
    renderables_.erase(renderables_.begin() + static_cast<std::ptrdiff_t>(index));
    renderable_ids_.erase(found);
    return true;
}

bool Scene::contains(RenderableId id) const noexcept {
    return std::ranges::find(renderable_ids_, id) != renderable_ids_.end();
}

Renderable& Scene::renderable(RenderableId id) {
    const auto found = std::ranges::find(renderable_ids_, id);
    if (found == renderable_ids_.end()) {
        throw std::out_of_range{"Unknown renderable identifier"};
    }
    return renderables_.at(
        static_cast<std::size_t>(std::distance(renderable_ids_.begin(), found)));
}

const Renderable& Scene::renderable(RenderableId id) const {
    const auto found = std::ranges::find(renderable_ids_, id);
    if (found == renderable_ids_.end()) {
        throw std::out_of_range{"Unknown renderable identifier"};
    }
    return renderables_.at(
        static_cast<std::size_t>(std::distance(renderable_ids_.begin(), found)));
}

std::span<const Renderable> Scene::renderables() const noexcept {
    return renderables_;
}

std::span<const RenderableId> Scene::renderable_ids() const noexcept {
    return renderable_ids_;
}

bool Scene::empty() const noexcept { return renderables_.empty(); }
std::size_t Scene::size() const noexcept { return renderables_.size(); }

} // namespace mgv
