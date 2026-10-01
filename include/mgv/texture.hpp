#pragma once

#include <cstdint>
#include <span>
#include <vector>

namespace mgv {

enum class PixelFormat {
    rgba8_unorm,
};

enum class ColorSpace {
    linear,
    srgb,
};

enum class TextureFilter {
    nearest,
    linear,
};

enum class TextureAddressMode {
    repeat,
    mirrored_repeat,
    clamp_to_edge,
};

struct SamplerDescriptor final {
    TextureFilter min_filter{TextureFilter::linear};
    TextureFilter mag_filter{TextureFilter::linear};
    TextureAddressMode address_u{TextureAddressMode::repeat};
    TextureAddressMode address_v{TextureAddressMode::repeat};

    friend bool operator==(const SamplerDescriptor&, const SamplerDescriptor&) = default;
};

class ImageData final {
public:
    ImageData(
        std::uint32_t width,
        std::uint32_t height,
        PixelFormat format,
        ColorSpace color_space,
        std::vector<std::uint8_t> pixels);

    [[nodiscard]] std::uint32_t width() const noexcept;
    [[nodiscard]] std::uint32_t height() const noexcept;
    [[nodiscard]] PixelFormat format() const noexcept;
    [[nodiscard]] ColorSpace color_space() const noexcept;
    [[nodiscard]] std::span<const std::uint8_t> pixels() const noexcept;

private:
    std::uint32_t width_;
    std::uint32_t height_;
    PixelFormat format_;
    ColorSpace color_space_;
    std::vector<std::uint8_t> pixels_;
};

class Texture final {
public:
    explicit Texture(ImageData image, SamplerDescriptor sampler = {});

    [[nodiscard]] const ImageData& image() const noexcept;
    [[nodiscard]] const SamplerDescriptor& sampler() const noexcept;

private:
    ImageData image_;
    SamplerDescriptor sampler_;
};

} // namespace mgv
