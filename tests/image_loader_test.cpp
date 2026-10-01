#include "mgv/image_loader.hpp"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include <filesystem>

TEST_CASE("image loader decodes files as RGBA while preserving color-space intent") {
    const auto path =
        std::filesystem::path{MGV_TEST_FIXTURES} / "material_model" / "textures" / "shared.ppm";

    const auto image = mgv::ImageLoader{}.load(path, mgv::ColorSpace::srgb);

    CHECK(image.width() == 2);
    CHECK(image.height() == 1);
    CHECK(image.format() == mgv::PixelFormat::rgba8_unorm);
    CHECK(image.color_space() == mgv::ColorSpace::srgb);
    const std::vector<std::uint8_t> pixels{image.pixels().begin(), image.pixels().end()};
    CHECK(pixels == std::vector<std::uint8_t>{255, 0, 0, 255, 0, 255, 0, 255});
}

TEST_CASE("image loader includes the file path in decode errors") {
    const auto path = std::filesystem::path{MGV_TEST_FIXTURES} / "missing.png";

    CHECK_THROWS_WITH(
        mgv::ImageLoader{}.load(path),
        Catch::Matchers::ContainsSubstring(path.string()));
}
