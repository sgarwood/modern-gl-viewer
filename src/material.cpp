#include "mgv/material.hpp"

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

} // namespace mgv
