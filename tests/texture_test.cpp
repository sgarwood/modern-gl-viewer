#include "mgv/material.hpp"
#include "mgv/texture.hpp"

#include <catch2/catch_test_macros.hpp>

#include <cstdint>
#include <memory>
#include <stdexcept>
#include <vector>

namespace {

[[nodiscard]] mgv::ImageData image(std::uint8_t red) {
    return mgv::ImageData{
        1,
        1,
        mgv::PixelFormat::rgba8_unorm,
        mgv::ColorSpace::srgb,
        {red, 20, 30, 255},
    };
}

[[nodiscard]] std::shared_ptr<const mgv::Material> material() {
    return std::make_shared<const mgv::Material>(
        mgv::ShaderSources{"vertex", "fragment", "test.vert", "test.frag"});
}

} // namespace

TEST_CASE("image data validates dimensions and byte count") {
    const auto valid = image(10);

    CHECK(valid.width() == 1);
    CHECK(valid.height() == 1);
    CHECK(valid.format() == mgv::PixelFormat::rgba8_unorm);
    CHECK(valid.color_space() == mgv::ColorSpace::srgb);
    CHECK(valid.pixels().size() == 4);

    CHECK_THROWS_AS(
        (mgv::ImageData{0, 1, mgv::PixelFormat::rgba8_unorm, mgv::ColorSpace::linear, {}}),
        std::invalid_argument);
    CHECK_THROWS_AS(
        (mgv::ImageData{1, 1, mgv::PixelFormat::rgba8_unorm, mgv::ColorSpace::linear, {1, 2, 3}}),
        std::invalid_argument);
}

TEST_CASE("texture owns validated image and sampler descriptions") {
    const mgv::SamplerDescriptor sampler{
        .min_filter = mgv::TextureFilter::nearest,
        .mag_filter = mgv::TextureFilter::linear,
        .address_u = mgv::TextureAddressMode::clamp_to_edge,
        .address_v = mgv::TextureAddressMode::repeat,
    };
    const mgv::Texture texture{image(10), sampler};

    CHECK(texture.image().pixels().front() == 10);
    CHECK(texture.sampler() == sampler);
}

TEST_CASE("material instance owns named texture bindings independently of its pipeline") {
    const auto first = std::make_shared<const mgv::Texture>(image(10));
    const auto replacement = std::make_shared<const mgv::Texture>(image(40));
    mgv::MaterialInstance instance{material()};

    instance.set_texture("uBaseColorTexture", first);
    instance.set_texture("uBaseColorTexture", replacement);

    REQUIRE(instance.texture_bindings().size() == 1);
    CHECK(instance.texture_bindings().front().name == "uBaseColorTexture");
    CHECK(instance.texture_bindings().front().texture == replacement);
    CHECK(instance.material()->pipeline().topology == mgv::PrimitiveTopology::triangle_list);
    CHECK_THROWS_AS(instance.set_texture("", replacement), std::invalid_argument);
    CHECK_THROWS_AS(instance.set_texture("uMissing", nullptr), std::invalid_argument);
    CHECK_THROWS_AS(mgv::MaterialInstance{nullptr}, std::invalid_argument);
}

TEST_CASE("material instance owns named color bindings") {
    mgv::MaterialInstance instance{material()};

    instance.set_color("uBaseColorFactor", {0.8F, 0.2F, 0.1F, 0.5F});
    instance.set_color("uBaseColorFactor", {0.1F, 0.2F, 0.3F, 1.0F});

    REQUIRE(instance.color_bindings().size() == 1);
    CHECK(instance.color_bindings().front().name == "uBaseColorFactor");
    CHECK(instance.color_bindings().front().value == mgv::Vec4{0.1F, 0.2F, 0.3F, 1.0F});
    CHECK_THROWS_AS(instance.set_color("", {}), std::invalid_argument);
}
