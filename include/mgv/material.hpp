#pragma once

#include "mgv/render_pipeline.hpp"

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

} // namespace mgv
