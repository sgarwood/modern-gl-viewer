#pragma once

#include "mgv/imported_model.hpp"

#include <filesystem>

namespace mgv {

/// Loads a glTF 2.0 model, including its skin.
///
/// Exists because OBJ cannot express a skeleton at all -- there is no such
/// thing as a rigged OBJ -- and a character is the one thing on a golf
/// course that has to deform.
///
/// Both the JSON form and the binary `.glb` container are accepted. Buffers
/// may be embedded as base64 data URIs, carried in a `.glb`'s binary chunk,
/// or sit in a separate file beside the document.
///
/// This is a reader for the subset a character needs: positions, normals,
/// texture coordinates, joints, weights, indices, base colour, and the skin.
/// Cameras, lights, morph targets, sparse accessors and the extension
/// ecosystem are not read, and a file using them loads without them rather
/// than failing.
class GltfLoader final {
public:
    [[nodiscard]] ImportedModel load(const std::filesystem::path& path) const;
};

} // namespace mgv
