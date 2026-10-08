#include "mgv/image_loader.hpp"

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#include <fstream>
#include <iterator>
#include <limits>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace mgv {
namespace {

[[nodiscard]] std::optional<ImageData> load_ascii_ppm(
    const std::filesystem::path& path,
    ColorSpace color_space) {
    std::ifstream input{path};
    std::string magic;
    input >> magic;
    if (magic != "P3") {
        return std::nullopt;
    }
    std::string contents{
        std::istreambuf_iterator<char>{input},
        std::istreambuf_iterator<char>{},
    };
    std::istringstream values{contents};
    std::string line;
    std::string without_comments;
    while (std::getline(values, line)) {
        const auto comment = line.find('#');
        without_comments.append(line.substr(0, comment));
        without_comments.push_back('\n');
    }
    std::istringstream pixels_input{without_comments};
    std::uint32_t width{};
    std::uint32_t height{};
    int maximum{};
    if (!(pixels_input >> width >> height >> maximum) || width == 0 || height == 0 || maximum != 255) {
        throw std::runtime_error{"Invalid PPM image header: " + path.string()};
    }
    const auto width_size = static_cast<std::size_t>(width);
    const auto height_size = static_cast<std::size_t>(height);
    if (width_size > std::numeric_limits<std::size_t>::max() / height_size / 4U) {
        throw std::runtime_error{"PPM image is too large: " + path.string()};
    }
    const auto pixel_count = width_size * height_size;
    std::vector<std::uint8_t> pixels;
    pixels.reserve(pixel_count * 4U);
    for (std::size_t index = 0; index < pixel_count; ++index) {
        int red{};
        int green{};
        int blue{};
        if (!(pixels_input >> red >> green >> blue) ||
            red < 0 || red > 255 || green < 0 || green > 255 || blue < 0 || blue > 255) {
            throw std::runtime_error{"Invalid PPM image pixels: " + path.string()};
        }
        pixels.push_back(static_cast<std::uint8_t>(red));
        pixels.push_back(static_cast<std::uint8_t>(green));
        pixels.push_back(static_cast<std::uint8_t>(blue));
        pixels.push_back(255);
    }
    return ImageData{width, height, PixelFormat::rgba8_unorm, color_space, std::move(pixels)};
}

} // namespace

struct ImageLoader::Impl final {
    /// Wraps whatever stb handed back, or explains why it did not.
    [[nodiscard]] static ImageData adopt(
        stbi_uc* decoded,
        int width,
        int height,
        ColorSpace color_space,
        std::string_view source_name) {
        constexpr int output_channels = 4;
        if (decoded == nullptr) {
            const auto* reason = stbi_failure_reason();
            throw std::runtime_error{
                "Unable to decode image '" + std::string{source_name} + "': " +
                (reason != nullptr ? reason : "unknown error")};
        }
        const auto release = [](stbi_uc* pixels) { stbi_image_free(pixels); };
        const std::unique_ptr<stbi_uc, decltype(release)> pixels{decoded, release};
        if (width <= 0 || height <= 0) {
            throw std::runtime_error{
                "Decoded image has invalid dimensions: " + std::string{source_name}};
        }
        const auto width_size = static_cast<std::size_t>(width);
        const auto height_size = static_cast<std::size_t>(height);
        if (width_size > std::numeric_limits<std::size_t>::max() / height_size /
                             static_cast<std::size_t>(output_channels)) {
            throw std::runtime_error{
                "Decoded image is too large: " + std::string{source_name}};
        }
        const auto byte_count = width_size * height_size * output_channels;
        return ImageData{
            static_cast<std::uint32_t>(width),
            static_cast<std::uint32_t>(height),
            PixelFormat::rgba8_unorm,
            color_space,
            std::vector<std::uint8_t>{pixels.get(), pixels.get() + byte_count},
        };
    }

    [[nodiscard]] ImageData load(const std::filesystem::path& path, ColorSpace color_space) const {
        int width{};
        int height{};
        int source_channels{};
        auto* decoded = stbi_load(path.string().c_str(), &width, &height, &source_channels, 4);
        if (decoded == nullptr) {
            if (auto ppm = load_ascii_ppm(path, color_space)) {
                return std::move(*ppm);
            }
        }
        return adopt(decoded, width, height, color_space, path.string());
    }

    [[nodiscard]] ImageData decode(
        std::span<const std::byte> bytes,
        ColorSpace color_space,
        std::string_view source_name) const {
        if (bytes.size() > static_cast<std::size_t>(std::numeric_limits<int>::max())) {
            throw std::runtime_error{
                "Encoded image is too large: " + std::string{source_name}};
        }
        int width{};
        int height{};
        int source_channels{};
        auto* decoded = stbi_load_from_memory(
            reinterpret_cast<const stbi_uc*>(bytes.data()),
            static_cast<int>(bytes.size()),
            &width,
            &height,
            &source_channels,
            4);
        return adopt(decoded, width, height, color_space, source_name);
    }
};

ImageLoader::ImageLoader() : impl_{std::make_unique<Impl>()} {}
ImageLoader::~ImageLoader() = default;
ImageLoader::ImageLoader(ImageLoader&&) noexcept = default;
ImageLoader& ImageLoader::operator=(ImageLoader&&) noexcept = default;

ImageData ImageLoader::load(const std::filesystem::path& path, ColorSpace color_space) const {
    return impl_->load(path, color_space);
}

ImageData ImageLoader::decode(
    std::span<const std::byte> bytes,
    ColorSpace color_space,
    std::string_view source_name) const {
    return impl_->decode(bytes, color_space, source_name);
}

} // namespace mgv
