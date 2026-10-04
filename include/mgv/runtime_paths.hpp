#pragma once

#include <filesystem>

namespace mgv {

// Prefer the source tree while developing, then fall back to the package layout.
[[nodiscard]] std::filesystem::path locate_asset_directory(
    const std::filesystem::path& build_asset_directory,
    const std::filesystem::path& executable_directory);

} // namespace mgv
