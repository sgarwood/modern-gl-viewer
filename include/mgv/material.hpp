#pragma once

#include "mgv/shader_loader.hpp"

namespace mgv {

class Material final {
public:
    explicit Material(ShaderSources shaders);

    [[nodiscard]] const ShaderSources& shaders() const noexcept;

private:
    ShaderSources shaders_;
};

} // namespace mgv
