#include "mgv/material.hpp"

#include <algorithm>
#include <stdexcept>
#include <utility>

namespace mgv {

Material::Material(ShaderSources shaders)
    : Material{RenderPipelineDescriptor{
          .shaders = std::move(shaders),
          .topology = PrimitiveTopology::triangle_list,
          .rasterization = {},
          .depth = {},
          .blending = {},
      }} {}

Material::Material(RenderPipelineDescriptor pipeline) : pipeline_{std::move(pipeline)} {
    if (pipeline_.shaders.vertex.empty() || pipeline_.shaders.fragment.empty()) {
        throw std::invalid_argument{"Material requires non-empty vertex and fragment shaders"};
    }
}

const ShaderSources& Material::shaders() const noexcept {
    return pipeline_.shaders;
}

const RenderPipelineDescriptor& Material::pipeline() const noexcept {
    return pipeline_;
}

MaterialInstance::MaterialInstance(std::shared_ptr<const Material> material)
    : material_{std::move(material)} {
    if (!material_) {
        throw std::invalid_argument{"Material instance requires a material"};
    }
}

MaterialInstance& MaterialInstance::set_texture(
    std::string name,
    std::shared_ptr<const Texture> texture) {
    if (name.empty()) {
        throw std::invalid_argument{"Texture binding name must not be empty"};
    }
    if (!texture) {
        throw std::invalid_argument{"Texture binding requires a texture"};
    }

    const auto existing = std::ranges::find(texture_bindings_, name, &MaterialTextureBinding::name);
    if (existing == texture_bindings_.end()) {
        texture_bindings_.push_back({std::move(name), std::move(texture)});
    } else {
        existing->texture = std::move(texture);
    }
    return *this;
}

MaterialInstance& MaterialInstance::set_color(std::string name, Vec4 value) {
    if (name.empty()) {
        throw std::invalid_argument{"Color binding name must not be empty"};
    }
    const auto existing = std::ranges::find(color_bindings_, name, &MaterialColorBinding::name);
    if (existing == color_bindings_.end()) {
        color_bindings_.push_back({std::move(name), value});
    } else {
        existing->value = value;
    }
    return *this;
}

const std::shared_ptr<const Material>& MaterialInstance::material() const noexcept {
    return material_;
}

std::span<const MaterialTextureBinding> MaterialInstance::texture_bindings() const noexcept {
    return texture_bindings_;
}

std::span<const MaterialColorBinding> MaterialInstance::color_bindings() const noexcept {
    return color_bindings_;
}

} // namespace mgv
