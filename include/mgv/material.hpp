#pragma once

#include "mgv/mesh.hpp"
#include "mgv/render_pipeline.hpp"
#include "mgv/texture.hpp"

#include <memory>
#include <span>
#include <string>
#include <vector>

namespace mgv {

class Material final {
public:
    explicit Material(ShaderSources shaders);
    explicit Material(RenderPipelineDescriptor pipeline);

    [[nodiscard]] const ShaderSources& shaders() const noexcept;
    [[nodiscard]] const RenderPipelineDescriptor& pipeline() const noexcept;

private:
    RenderPipelineDescriptor pipeline_;
};

struct MaterialTextureBinding final {
    std::string name;
    std::shared_ptr<const Texture> texture;
};

struct MaterialColorBinding final {
    std::string name;
    Vec4 value;
};

class MaterialInstance final {
public:
    explicit MaterialInstance(std::shared_ptr<const Material> material);

    MaterialInstance& set_texture(std::string name, std::shared_ptr<const Texture> texture);
    MaterialInstance& set_color(std::string name, Vec4 value);

    [[nodiscard]] const std::shared_ptr<const Material>& material() const noexcept;
    [[nodiscard]] std::span<const MaterialTextureBinding> texture_bindings() const noexcept;
    [[nodiscard]] std::span<const MaterialColorBinding> color_bindings() const noexcept;

private:
    std::shared_ptr<const Material> material_;
    std::vector<MaterialTextureBinding> texture_bindings_;
    std::vector<MaterialColorBinding> color_bindings_;
};

} // namespace mgv
