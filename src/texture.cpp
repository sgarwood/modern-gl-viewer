#include "mgv/texture.hpp"

#include <limits>
#include <stdexcept>
#include <utility>

namespace mgv {
namespace {

constexpr std::size_t rgba8_byte_count = 4;

[[nodiscard]] std::size_t expected_size(std::uint32_t width, std::uint32_t height) {
    if (width == 0 || height == 0) {
        throw std::invalid_argument{"Image dimensions must be positive"};
    }
    const auto width_size = static_cast<std::size_t>(width);
    const auto height_size = static_cast<std::size_t>(height);
    if (width_size > std::numeric_limits<std::size_t>::max() / height_size / rgba8_byte_count) {
        throw std::overflow_error{"Image dimensions exceed addressable memory"};
    }
    return width_size * height_size * rgba8_byte_count;
}

} // namespace

ImageData::ImageData(
    std::uint32_t width,
    std::uint32_t height,
    PixelFormat format,
    ColorSpace color_space,
    std::vector<std::uint8_t> pixels)
    : width_{width},
      height_{height},
      format_{format},
      color_space_{color_space},
      pixels_{std::move(pixels)} {
    if (pixels_.size() != expected_size(width_, height_)) {
        throw std::invalid_argument{"RGBA8 image byte count does not match its dimensions"};
    }
}

std::uint32_t ImageData::width() const noexcept { return width_; }
std::uint32_t ImageData::height() const noexcept { return height_; }
PixelFormat ImageData::format() const noexcept { return format_; }
ColorSpace ImageData::color_space() const noexcept { return color_space_; }
std::span<const std::uint8_t> ImageData::pixels() const noexcept { return pixels_; }

Texture::Texture(ImageData image, SamplerDescriptor sampler)
    : image_{std::move(image)}, sampler_{sampler} {}

const ImageData& Texture::image() const noexcept { return image_; }
const SamplerDescriptor& Texture::sampler() const noexcept { return sampler_; }

} // namespace mgv
