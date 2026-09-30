#include "mgv/obj_loader.hpp"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include <filesystem>
#include <sstream>
#include <stdexcept>

TEST_CASE("OBJ loader reads positions, UVs, normals, and indices") {
    const auto path = std::filesystem::path{MGV_TEST_FIXTURES} / "triangle.obj";
    const auto mesh = mgv::ObjLoader{}.load(path);

    REQUIRE(mesh.vertices.size() == 3);
    REQUIRE(mesh.indices == std::vector<std::uint32_t>{0, 1, 2});
    CHECK(mesh.vertices[0].position == mgv::Vec3{0.0F, 1.0F, 0.0F});
    CHECK(mesh.vertices[0].tex_coord == mgv::Vec2{0.5F, 1.0F});
    CHECK(mesh.vertices[0].normal == mgv::Vec3{0.0F, 0.0F, 1.0F});
}

TEST_CASE("OBJ loader triangulates polygons and resolves negative indices") {
    std::istringstream obj{
        "v -1 -1 0\n"
        "v  1 -1 0\n"
        "v  1  1 0\n"
        "v -1  1 0\n"
        "f -4 -3 -2 -1\n"};

    const auto mesh = mgv::ObjLoader{}.parse(obj, "quad.obj");

    REQUIRE(mesh.vertices.size() == 4);
    CHECK(mesh.indices == std::vector<std::uint32_t>{0, 1, 2, 0, 2, 3});
    for (const auto& vertex : mesh.vertices) {
        CHECK(vertex.normal == mgv::Vec3{0.0F, 0.0F, 1.0F});
    }
}

TEST_CASE("OBJ loader reports the source and line for an invalid face") {
    std::istringstream obj{"v 0 0 0\nf 1 2 3\n"};

    CHECK_THROWS_WITH(
        mgv::ObjLoader{}.parse(obj, "broken.obj"),
        Catch::Matchers::ContainsSubstring("broken.obj:2"));
}

TEST_CASE("OBJ loader preserves separate vertices when texture coordinates differ") {
    std::istringstream obj{
        "v 0 0 0\n"
        "v 1 0 0\n"
        "v 0 1 0\n"
        "vt 0 0\n"
        "vt 1 0\n"
        "vt 0 1\n"
        "vt 1 1\n"
        "f 1/1 2/2 3/3\n"
        "f 1/4 3/3 2/2\n"};

    const auto mesh = mgv::ObjLoader{}.parse(obj, "seam.obj");

    REQUIRE(mesh.vertices.size() == 4);
    CHECK(mesh.vertices[0].position == mesh.vertices[3].position);
    CHECK(mesh.vertices[0].tex_coord != mesh.vertices[3].tex_coord);
}

TEST_CASE("OBJ loader rejects a file without renderable faces") {
    std::istringstream obj{"v 0 0 0\n"};

    CHECK_THROWS_WITH(
        mgv::ObjLoader{}.parse(obj, "empty.obj"),
        Catch::Matchers::ContainsSubstring("no renderable faces"));
}
