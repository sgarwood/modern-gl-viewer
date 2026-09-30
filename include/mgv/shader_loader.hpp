#pragma once

#include <filesystem>
#include <string>

namespace mgv {

struct ShaderSources final {
    std::string vertex;
    std::string fragment;
    std::string vertex_name;
    std::string fragment_name;
};

class ShaderLoader final {
public:
    [[nodiscard]] static ShaderSources load(
        const std::filesystem::path& vertex_path,
        const std::filesystem::path& fragment_path);
};

} // namespace mgv
