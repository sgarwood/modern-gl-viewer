#include "mgv/scene.hpp"

#include <stdexcept>
#include <utility>

namespace mgv {

Renderable::Renderable(
    std::shared_ptr<const MeshData> mesh,
    std::shared_ptr<const Material> material,
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
const std::shared_ptr<const Material>& Renderable::material() const noexcept { return material_; }
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

Renderable& Scene::add(Renderable renderable) {
    renderables_.push_back(std::move(renderable));
    return renderables_.back();
}

std::span<const Renderable> Scene::renderables() const noexcept {
    return renderables_;
}

bool Scene::empty() const noexcept { return renderables_.empty(); }
std::size_t Scene::size() const noexcept { return renderables_.size(); }

} // namespace mgv
