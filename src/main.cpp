#include "mgv/application.hpp"

#include <exception>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string_view>

namespace {

struct Arguments final {
    std::filesystem::path model{std::filesystem::path{MGV_DEFAULT_ASSET_DIR} / "ball.obj"};
    std::filesystem::path vertex{std::filesystem::path{MGV_DEFAULT_ASSET_DIR} / "shaders/default.vert"};
    std::filesystem::path fragment{std::filesystem::path{MGV_DEFAULT_ASSET_DIR} / "shaders/default.frag"};
};

void print_usage(std::string_view executable) {
    std::cout << "Usage: " << executable << " [model.obj] [--vertex shader.vert] [--fragment shader.frag]\n";
}

[[nodiscard]] Arguments parse_arguments(int argc, char** argv) {
    Arguments result;
    bool model_set = false;
    for (int index = 1; index < argc; ++index) {
        const std::string_view argument{argv[index]};
        if (argument == "--help" || argument == "-h") {
            print_usage(argv[0]);
            std::exit(0);
        }
        if (argument == "--vertex" || argument == "--fragment") {
            if (++index >= argc) {
                throw std::invalid_argument{"Missing path after " + std::string{argument}};
            }
            (argument == "--vertex" ? result.vertex : result.fragment) = argv[index];
            continue;
        }
        if (!model_set) {
            result.model = argument;
            model_set = true;
            continue;
        }
        throw std::invalid_argument{"Unexpected argument: " + std::string{argument}};
    }
    return result;
}

} // namespace

int main(int argc, char** argv) {
    try {
        const auto arguments = parse_arguments(argc, argv);
        auto application = mgv::Application::Builder{}
                               .title("Modern GL Viewer")
                               .size(1280, 720)
                               .model(arguments.model)
                               .shaders(arguments.vertex, arguments.fragment)
                               .build();
        return application.run();
    } catch (const std::exception& error) {
        std::cerr << "mgv: " << error.what() << '\n';
        return 1;
    }
}
