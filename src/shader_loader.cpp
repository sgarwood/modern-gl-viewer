#include "mgv/shader_loader.hpp"

#include <fstream>
#include <iterator>
#include <stdexcept>

namespace mgv {
namespace {

[[nodiscard]] std::string read_source(const std::filesystem::path& path) {
    std::ifstream input{path, std::ios::binary};
    if (!input) {
        throw std::runtime_error{"Unable to open shader: " + path.string()};
    }

    std::string source{std::istreambuf_iterator<char>{input}, std::istreambuf_iterator<char>{}};
    if (source.empty()) {
        throw std::runtime_error{"Shader source is empty: " + path.string()};
    }
    if (!input.eof() && input.fail()) {
        throw std::runtime_error{"Unable to read shader: " + path.string()};
    }
    return source;
}

} // namespace

ShaderSources ShaderLoader::load(
    const std::filesystem::path& vertex_path,
    const std::filesystem::path& fragment_path) {
    return {
        .vertex = read_source(vertex_path),
        .fragment = read_source(fragment_path),
        .vertex_name = vertex_path.string(),
        .fragment_name = fragment_path.string(),
    };
}

} // namespace mgv
