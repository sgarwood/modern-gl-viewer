#include "mgv/runtime_paths.hpp"

#include <catch2/catch_test_macros.hpp>

#include <filesystem>

TEST_CASE("runtime assets use an existing development directory") {
    const auto fixtures = std::filesystem::path{MGV_TEST_FIXTURES};

    REQUIRE(mgv::locate_asset_directory(fixtures, "ignored/bin") == fixtures);
}

TEST_CASE("runtime assets fall back to the relocatable install layout") {
    const auto executable_directory = std::filesystem::path{"package-root"} / "bin";
    const auto missing_source = std::filesystem::path{"a-directory-that-does-not-exist"};

    REQUIRE(
        mgv::locate_asset_directory(missing_source, executable_directory)
        == std::filesystem::path{"package-root/share/modern-gl-viewer/assets"});
}
