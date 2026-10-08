#pragma once

#include "mgv/mesh.hpp"

#include <filesystem>

namespace mgv {

/// Reads a binary STL mesh.
///
/// STL is what CAD and sculpting tools export when asked for something
/// simple: a flat list of triangles with a face normal each, no indices, no
/// texture coordinates and no materials. It carries less than the OBJ and
/// glTF importers expect, which is the whole reason it is read separately
/// rather than folded into either.
///
/// \param crease_degrees Angle beyond which an edge stays sharp. STL
///        duplicates every vertex, so a mesh read literally is faceted;
///        welding corners and averaging their normals recovers the smooth
///        surfaces the exporter meant, and the crease angle is what keeps a
///        club face from melting into its sole while doing it.
[[nodiscard]] MeshData load_binary_stl(
    const std::filesystem::path& path,
    float crease_degrees = 40.0F);

} // namespace mgv
