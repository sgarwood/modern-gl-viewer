#include <glad/glad.h>
#include <vector>
#include <string>
#include <stdexcept>
#include <iostream>

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

void save_framebuffer_to_png(int width, int height, const std::string& path) {
    std::vector<unsigned char> pixels(width * height * 4);
    glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
    
    // OpenGL bottom-up to top-down
    std::vector<unsigned char> flipped(width * height * 4);
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width * 4; ++x) {
            flipped[(height - 1 - y) * width * 4 + x] = pixels[y * width * 4 + x];
        }
    }
    
    if (!stbi_write_png(path.c_str(), width, height, 4, flipped.data(), width * 4)) {
        std::cerr << "Failed to write framebuffer to " << path << '\n';
    } else {
        std::cout << "Saved framebuffer to " << path << '\n';
    }
}
