#include "mgv/image_loader.hpp"

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#include <fstream>
#include <iterator>
#include <limits>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
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
    [[nodiscard]] ImageData load(const std::filesystem::path& path, ColorSpace color_space) const {
        int width{};
        int height{};
        int source_channels{};
        constexpr int output_channels = 4;
        auto* decoded = stbi_load(
            path.string().c_str(),
            &width,
            &height,
            &source_channels,
            output_channels);
        if (decoded == nullptr) {
            if (auto ppm = load_ascii_ppm(path, color_space)) {
                return std::move(*ppm);
            }
            const auto* reason = stbi_failure_reason();
            throw std::runtime_error{
                "Unable to decode image '" + path.string() + "': " +
                (reason != nullptr ? reason : "unknown error")};
        }
        const auto release = [](stbi_uc* pixels) { stbi_image_free(pixels); };
        const std::unique_ptr<stbi_uc, decltype(release)> pixels{decoded, release};
        if (width <= 0 || height <= 0) {
            throw std::runtime_error{"Decoded image has invalid dimensions: " + path.string()};
        }
        const auto width_size = static_cast<std::size_t>(width);
        const auto height_size = static_cast<std::size_t>(height);
        if (width_size > std::numeric_limits<std::size_t>::max() / height_size /
                             static_cast<std::size_t>(output_channels)) {
            throw std::runtime_error{"Decoded image is too large: " + path.string()};
        }
        const auto pixel_count = width_size * height_size;
        const auto byte_count = pixel_count * output_channels;
        return ImageData{
            static_cast<std::uint32_t>(width),
            static_cast<std::uint32_t>(height),
            PixelFormat::rgba8_unorm,
            color_space,
            std::vector<std::uint8_t>{pixels.get(), pixels.get() + byte_count},
        };
    }
};

ImageLoader::ImageLoader() : impl_{std::make_unique<Impl>()} {}
ImageLoader::~ImageLoader() = default;
ImageLoader::ImageLoader(ImageLoader&&) noexcept = default;
ImageLoader& ImageLoader::operator=(ImageLoader&&) noexcept = default;

ImageData ImageLoader::load(const std::filesystem::path& path, ColorSpace color_space) const {
    return impl_->load(path, color_space);
}

} // namespace mgv
