#pragma once

#include "mgv/mesh.hpp"

#include <cstddef>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace mgv {

struct ImportedMaterial final {
    std::string name;
    Vec3 diffuse_color{1.0F, 1.0F, 1.0F};
    float opacity{1.0F};
    std::optional<std::filesystem::path> diffuse_texture;
};

struct ImportedPrimitive final {
    MeshData mesh;
    std::optional<std::size_t> material_index;
};

struct ImportedModel final {
    std::vector<ImportedPrimitive> primitives;
    std::vector<ImportedMaterial> materials;
};

} // namespace mgv
