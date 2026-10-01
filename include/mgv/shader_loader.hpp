#pragma once

#include <filesystem>
#include <string>

namespace mgv {

enum class ShaderSourceLanguage {
    glsl,
    hlsl,
};

struct ShaderSources final {
    std::string vertex;
    std::string fragment;
    std::string vertex_name;
    std::string fragment_name;
    ShaderSourceLanguage language{ShaderSourceLanguage::glsl};

    friend bool operator==(const ShaderSources&, const ShaderSources&) = default;
};

class ShaderLoader final {
public:
    [[nodiscard]] static ShaderSources load(
        const std::filesystem::path& vertex_path,
        const std::filesystem::path& fragment_path);
};

} // namespace mgv
