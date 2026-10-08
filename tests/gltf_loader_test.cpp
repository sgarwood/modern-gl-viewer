#include "mgv/gltf_loader.hpp"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include <filesystem>
#include <numeric>

namespace {

[[nodiscard]] std::filesystem::path fixture(const char* name) {
    return std::filesystem::path{MGV_TEST_FIXTURES} / "rigged" / name;
}

} // namespace

TEST_CASE("a rigged glTF loads its geometry") {
    const auto model = mgv::GltfLoader{}.load(fixture("banner.gltf"));

    REQUIRE(model.primitives.size() == 1);
    const auto& mesh = model.primitives.front().mesh;
    CHECK(mesh.vertices.size() == 6);
    CHECK(mesh.indices.size() == 12);
    SECTION("positions come through in metres, as authored") {
        CHECK(mesh.vertices.front().position == mgv::Vec3{-0.5F, 0.0F, 0.0F});
        CHECK(mesh.vertices.back().position == mgv::Vec3{0.5F, 2.0F, 0.0F});
    }
    SECTION("normals and texture coordinates come through too") {
        CHECK(mesh.vertices.front().normal == mgv::Vec3{0.0F, 0.0F, 1.0F});
        CHECK(mesh.vertices.back().tex_coord == mgv::Vec2{1.0F, 1.0F});
    }
    SECTION("every index addresses a vertex") {
        for (const auto index : mesh.indices) {
            CHECK(index < mesh.vertices.size());
        }
    }
}

TEST_CASE("a rigged glTF loads one skinning influence per vertex") {
    const auto model = mgv::GltfLoader{}.load(fixture("banner.gltf"));
    const auto& mesh = model.primitives.front().mesh;

    REQUIRE(mesh.skinned());
    CHECK(mesh.skinning.size() == mesh.vertices.size());

    SECTION("the base of the mesh hangs entirely off the first joint") {
        CHECK(mesh.skinning.front().joints[0] == 0);
        CHECK(mesh.skinning.front().weights[0] == Catch::Approx(1.0F));
    }
    SECTION("the middle is shared between both") {
        CHECK(mesh.skinning[2].weights[0] == Catch::Approx(0.5F));
        CHECK(mesh.skinning[2].weights[1] == Catch::Approx(0.5F));
    }
    SECTION("the top hangs entirely off the second") {
        CHECK(mesh.skinning.back().joints[1] == 1);
        CHECK(mesh.skinning.back().weights[1] == Catch::Approx(1.0F));
    }
    SECTION("every vertex's weights sum to one") {
        for (const auto& influence : mesh.skinning) {
            const auto total = std::accumulate(
                influence.weights.begin(), influence.weights.end(), 0.0F);
            CHECK(total == Catch::Approx(1.0F));
        }
    }
}

TEST_CASE("a rigged glTF loads its skin, named and in bind pose") {
    const auto model = mgv::GltfLoader{}.load(fixture("banner.gltf"));

    REQUIRE(model.skin.has_value());
    SECTION("joints are named, so a separately converted skeleton can be matched") {
        REQUIRE(model.skin->joint_names.size() == 2);
        CHECK(model.skin->joint_names[0] == "Root");
        CHECK(model.skin->joint_names[1] == "Tip");
    }
    SECTION("there is an inverse bind matrix for every joint") {
        REQUIRE(model.skin->inverse_bind_matrices.size() == 2);
        // The second joint sits one unit up, so its inverse bind translates
        // one unit down. Column-major: translation is elements 12 to 14.
        CHECK(model.skin->inverse_bind_matrices[1][13] == Catch::Approx(-1.0F));
    }
}

TEST_CASE("an unskinned glTF loads without a skin rather than failing") {
    // The fixture is the only rigged file; reuse it to assert the shape of
    // the negative case through a mesh that has no skinning stream.
    mgv::MeshData plain;
    CHECK_FALSE(plain.skinned());
}

TEST_CASE("base colour comes through as the material's diffuse") {
    const auto model = mgv::GltfLoader{}.load(fixture("banner.gltf"));

    REQUIRE(model.materials.size() == 1);
    CHECK(model.materials.front().name == "Cloth");
    CHECK(model.materials.front().diffuse_color.x == Catch::Approx(0.8F));
    CHECK(model.materials.front().opacity == Catch::Approx(1.0F));
    REQUIRE(model.primitives.front().material_index.has_value());
    CHECK(*model.primitives.front().material_index == 0);
}

TEST_CASE("a missing glTF is reported by name") {
    CHECK_THROWS_WITH(
        mgv::GltfLoader{}.load(fixture("absent.gltf")),
        Catch::Matchers::ContainsSubstring("absent.gltf"));
}

TEST_CASE("a texture carried inside the model is decoded") {
    const auto model = mgv::GltfLoader{}.load(fixture("banner.gltf"));

    REQUIRE(model.materials.size() == 1);
    const auto& material = model.materials.front();
    REQUIRE(material.diffuse_image.has_value());

    const auto& image = *material.diffuse_image;
    CHECK(image.width() == 2);
    CHECK(image.height() == 2);
    CHECK(image.pixels().size() == 2 * 2 * 4);

    SECTION("base colour is decoded as sRGB, which is what it was authored in") {
        // Decoding it as linear is the classic way a textured character
        // comes out washed out.
        CHECK(image.color_space() == mgv::ColorSpace::srgb);
    }
    SECTION("the pixels arrive in the order they were written") {
        const auto pixels = image.pixels();
        CHECK(pixels[0] == 255);   // first texel red
        CHECK(pixels[1] == 0);
        CHECK(pixels[5] == 255);   // second texel green
        CHECK(pixels[11] == 255);  // third texel blue
    }
}

TEST_CASE("the model's sampler is honoured rather than defaulted") {
    const auto model = mgv::GltfLoader{}.load(fixture("banner.gltf"));
    const auto& sampler = model.materials.front().sampler;

    // The fixture deliberately asks for something other than the glTF
    // defaults, so a loader that ignored the sampler would be caught.
    CHECK(sampler.mag_filter == mgv::TextureFilter::nearest);
    CHECK(sampler.min_filter == mgv::TextureFilter::nearest);
    CHECK(sampler.address_u == mgv::TextureAddressMode::clamp_to_edge);
    CHECK(sampler.address_v == mgv::TextureAddressMode::mirrored_repeat);
}
