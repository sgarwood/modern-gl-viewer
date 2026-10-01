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

TEST_CASE("OBJ loader imports material primitives and resolves MTL texture paths") {
    const auto path = std::filesystem::path{MGV_TEST_FIXTURES} / "material_model" / "model.obj";

    const auto model = mgv::ObjLoader{}.load_model(path);

    REQUIRE(model.primitives.size() == 2);
    REQUIRE(model.materials.size() == 2);
    CHECK(model.primitives[0].material_index == 0);
    CHECK(model.primitives[1].material_index == 1);
    CHECK(model.primitives[0].mesh.indices.size() == 3);
    CHECK(model.primitives[1].mesh.indices.size() == 3);
    CHECK(model.materials[0].name == "Warm");
    CHECK(model.materials[0].diffuse_color == mgv::Vec3{0.8F, 0.2F, 0.1F});
    CHECK(model.materials[0].opacity == 1.0F);
    REQUIRE(model.materials[0].diffuse_texture.has_value());
    CHECK(*model.materials[0].diffuse_texture ==
          (path.parent_path() / "textures" / "shared.ppm").lexically_normal());
    CHECK(model.materials[1].name == "Cool");
    CHECK(model.materials[1].opacity == 0.5F);
    CHECK(model.materials[1].diffuse_texture == model.materials[0].diffuse_texture);
}

TEST_CASE("OBJ loader reports an unknown referenced material") {
    const auto path = std::filesystem::path{MGV_TEST_FIXTURES} / "unknown_material.obj";

    CHECK_THROWS_WITH(
        mgv::ObjLoader{}.load_model(path),
        Catch::Matchers::ContainsSubstring("MissingMaterial"));
}
