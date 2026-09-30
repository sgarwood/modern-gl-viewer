#include "mgv/material.hpp"

#include <stdexcept>
#include <utility>

namespace mgv {

Material::Material(ShaderSources shaders) : shaders_{std::move(shaders)} {
    if (shaders_.vertex.empty() || shaders_.fragment.empty()) {
        throw std::invalid_argument{"Material requires non-empty vertex and fragment shaders"};
    }
}

const ShaderSources& Material::shaders() const noexcept {
    return shaders_;
}

} // namespace mgv
