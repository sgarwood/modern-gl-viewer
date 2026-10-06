#include "mgv/screenshot.hpp"

#include <glad/gl.h>

#include <cstddef>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb_image_write.h>

namespace mgv {

void save_framebuffer_to_png(int width, int height, const std::string& path) {
    if (width <= 0 || height <= 0) {
        throw std::invalid_argument{"Framebuffer dimensions must be positive"};
    }
    const auto pixel_width = static_cast<std::size_t>(width);
    const auto pixel_height = static_cast<std::size_t>(height);
    constexpr std::size_t channel_count{4};
    if (pixel_height >
        std::numeric_limits<std::size_t>::max() / pixel_width / channel_count) {
        throw std::overflow_error{"Framebuffer dimensions exceed addressable storage"};
    }
    const auto byte_count = pixel_width * pixel_height * channel_count;
    std::vector<unsigned char> pixels(byte_count);
    glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());

    // OpenGL bottom-up to top-down
    std::vector<unsigned char> flipped(byte_count);
    for (int y = 0; y < height; ++y) {
        const auto source_row = static_cast<std::size_t>(y) * pixel_width * channel_count;
        const auto target_row = static_cast<std::size_t>(height - 1 - y) *
                                pixel_width * channel_count;
        for (std::size_t x = 0; x < pixel_width * channel_count; ++x) {
            flipped[target_row + x] = pixels[source_row + x];
        }
    }

    if (!stbi_write_png(path.c_str(), width, height, 4, flipped.data(), width * 4)) {
        std::cerr << "Failed to write framebuffer to " << path << '\n';
    } else {
        std::cout << "Saved framebuffer to " << path << '\n';
    }
}

} // namespace mgv
