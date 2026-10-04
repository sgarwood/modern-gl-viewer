#include "mgv/runtime_paths.hpp"

#include <system_error>

namespace mgv {

std::filesystem::path locate_asset_directory(
    const std::filesystem::path& build_asset_directory,
    const std::filesystem::path& executable_directory) {
    std::error_code error;
    if (std::filesystem::is_directory(build_asset_directory, error)) {
        return build_asset_directory;
    }

    return (executable_directory / ".." / "share" / "modern-gl-viewer" / "assets")
        .lexically_normal();
}

} // namespace mgv
