#pragma once

#include "mgv/texture.hpp"

#include <cstddef>
#include <filesystem>
#include <memory>
#include <span>
#include <string_view>

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

    /// Decodes an encoded image already in memory.
    ///
    /// A glTF binary carries its textures inside the file, so there is no
    /// path to hand a file-based decoder. `source_name` appears in the error
    /// if the bytes do not decode.
    [[nodiscard]] ImageData decode(
        std::span<const std::byte> bytes,
        ColorSpace color_space = ColorSpace::srgb,
        std::string_view source_name = "<memory>") const;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace mgv
