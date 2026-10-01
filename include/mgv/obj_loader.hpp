#pragma once

#include "mgv/imported_model.hpp"
#include "mgv/mesh.hpp"

#include <filesystem>
#include <iosfwd>
#include <memory>
#include <string_view>

namespace mgv {

class ObjLoader final {
public:
    ObjLoader();
    ~ObjLoader();

    ObjLoader(ObjLoader&&) noexcept;
    ObjLoader& operator=(ObjLoader&&) noexcept;
    ObjLoader(const ObjLoader&) = delete;
    ObjLoader& operator=(const ObjLoader&) = delete;

    [[nodiscard]] MeshData load(const std::filesystem::path& path) const;
    [[nodiscard]] ImportedModel load_model(const std::filesystem::path& path) const;
    [[nodiscard]] MeshData parse(std::istream& input, std::string_view source_name = "<stream>") const;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace mgv
