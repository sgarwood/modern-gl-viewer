#pragma once

#include "mgv/camera.hpp"
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

/// The bind-pose data a skinned mesh needs beyond its vertices.
struct ImportedSkin final {
    /// Names of the joints, in the order `SkinningVertex::joints` indexes
    /// them.
    ///
    /// Binding is by name, deliberately. A skeleton converted separately --
    /// by `gltf2ozz`, say -- has no reason to order its joints the way the
    /// mesh's skin does, and assuming the two agree produces a character
    /// that animates almost correctly, which is far worse to diagnose than
    /// one that does not animate at all.
    std::vector<std::string> joint_names;
    /// Inverse bind matrix per joint, in the same order. Column-major, as
    /// everything else here is.
    std::vector<Mat4> inverse_bind_matrices;
};

struct ImportedModel final {
    std::vector<ImportedPrimitive> primitives;
    std::vector<ImportedMaterial> materials;
    /// Present when the model carried a skin.
    std::optional<ImportedSkin> skin;
};

} // namespace mgv
