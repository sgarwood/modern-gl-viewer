#pragma once

#include "mgv/texture.hpp"

#include <filesystem>
#include <memory>

namespace mgv {

class ImageLoader final {
public:
    ImageLoader();
    ~ImageLoader();

    ImageLoader(ImageLoader&&) noexcept;
    ImageLoader& operator=(ImageLoader&&) noexcept;
    ImageLoader(const ImageLoader&) = delete;
    ImageLoader& operator=(const ImageLoader&) = delete;

    [[nodiscard]] ImageData load(
        const std::filesystem::path& path,
        ColorSpace color_space = ColorSpace::srgb) const;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace mgv
