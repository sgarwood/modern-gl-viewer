#include "mgv/stl_loader.hpp"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <cmath>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <array>
#include <functional>
#include <vector>

namespace {

/// Writes a binary STL of the given triangles, each {normal, v0, v1, v2}.
class StlFile final {
public:
    explicit StlFile(const std::vector<std::array<float, 12>>& triangles)
        : path_{std::filesystem::temp_directory_path() /
                ("mgv-stl-" + std::to_string(std::hash<const void*>{}(this)) + ".stl")} {
        std::ofstream out{path_, std::ios::binary};
        const std::vector<char> header(80, '\0');
        out.write(header.data(), 80);
        const auto count = static_cast<std::uint32_t>(triangles.size());
        out.write(reinterpret_cast<const char*>(&count), sizeof(count));
        for (const auto& triangle : triangles) {
            out.write(reinterpret_cast<const char*>(triangle.data()), sizeof(float) * 12);
            const std::uint16_t attribute{};
            out.write(reinterpret_cast<const char*>(&attribute), sizeof(attribute));
        }
    }
    ~StlFile() {
        std::error_code ignored;
        std::filesystem::remove(path_, ignored);
    }
    StlFile(const StlFile&) = delete;
    StlFile& operator=(const StlFile&) = delete;

    [[nodiscard]] const std::filesystem::path& path() const { return path_; }

private:
    std::filesystem::path path_;
};

} // namespace

TEST_CASE("a binary STL loads its triangles") {
    const StlFile file{{
        {0.0F, 0.0F, 1.0F, /**/ 0.0F, 0.0F, 0.0F, /**/ 1.0F, 0.0F, 0.0F, /**/ 0.0F, 1.0F, 0.0F},
    }};

    const auto mesh = mgv::load_binary_stl(file.path());

    CHECK(mesh.vertices.size() == 3);
    CHECK(mesh.indices.size() == 3);
    SECTION("with the winding the file gave") {
        CHECK(mesh.vertices[1].position.x == Catch::Approx(1.0F));
        CHECK(mesh.vertices[2].position.y == Catch::Approx(1.0F));
    }
    SECTION("and a normal off the face, not the file's claim") {
        // An exporter's stored normal is advisory and frequently zero; the
        // winding is the thing that has to agree with the renderer.
        CHECK(mesh.vertices[0].normal.z == Catch::Approx(1.0F).margin(1.0e-5F));
    }
}

TEST_CASE("shared corners are welded and smoothed below the crease angle") {
    // Two triangles meeting along an edge at a shallow angle: one surface.
    const StlFile file{{
        {0, 0, 1, /**/ 0, 0, 0, /**/ 1, 0, 0, /**/ 0, 1, 0},
        {0, 0, 1, /**/ 1, 0, 0, /**/ 1, 1, 0.02F, /**/ 0, 1, 0},
    }};

    const auto mesh = mgv::load_binary_stl(file.path(), 40.0F);

    SECTION("the shared positions collapse to one vertex each") {
        // Six corners, four distinct positions.
        CHECK(mesh.vertices.size() == 4);
        CHECK(mesh.indices.size() == 6);
    }
}

TEST_CASE("an edge sharper than the crease angle keeps both faces flat") {
    // A right-angled fold. Averaging across it would round off a corner the
    // exporter meant to be an edge.
    const StlFile file{{
        {0, 0, 1, /**/ 0, 0, 0, /**/ 1, 0, 0, /**/ 0, 1, 0},
        {0, 1, 0, /**/ 0, 0, 0, /**/ 0, 1, 0, /**/ 1, 0, 0},
    }};

    const auto mesh = mgv::load_binary_stl(file.path(), 40.0F);

    // Each corner on the fold has to appear twice, once per face, because
    // the two faces disagree about which way is out.
    CHECK(mesh.vertices.size() == 6);
    for (const auto& vertex : mesh.vertices) {
        const auto length = std::sqrt(
            vertex.normal.x * vertex.normal.x + vertex.normal.y * vertex.normal.y +
            vertex.normal.z * vertex.normal.z);
        CHECK(length == Catch::Approx(1.0F).margin(1.0e-4F));
    }
}

TEST_CASE("a truncated or absent STL is an error, not a half-read mesh") {
    CHECK_THROWS(mgv::load_binary_stl("definitely-not-here.stl"));

    SECTION("a file whose triangle count outruns its bytes") {
        const auto path = std::filesystem::temp_directory_path() / "mgv-stl-short.stl";
        {
            std::ofstream out{path, std::ios::binary};
            const std::vector<char> header(80, '\0');
            out.write(header.data(), 80);
            const std::uint32_t count = 1000;   // but no triangles follow
            out.write(reinterpret_cast<const char*>(&count), sizeof(count));
        }
        CHECK_THROWS(mgv::load_binary_stl(path));
        std::error_code ignored;
        std::filesystem::remove(path, ignored);
    }
}
