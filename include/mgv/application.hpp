#pragma once

#include "mgv/renderer.hpp"

#include <memory>
#include <string>

namespace mgv {

class Application final {
public:
    class Builder;

    ~Application();
    Application(Application&&) noexcept;
    Application& operator=(Application&&) noexcept;
    Application(const Application&) = delete;
    Application& operator=(const Application&) = delete;

    int run();

private:
    struct Config;
    struct Impl;

    explicit Application(Config config);
    std::unique_ptr<Impl> impl_;
};

class Application::Builder final {
public:
    Builder();
    ~Builder();
    Builder(Builder&&) noexcept;
    Builder& operator=(Builder&&) noexcept;
    Builder(const Builder&) = delete;
    Builder& operator=(const Builder&) = delete;

    Builder& title(std::string value);
    Builder& size(int width, int height);
    Builder& model(std::filesystem::path path);
    Builder& shaders(std::filesystem::path vertex, std::filesystem::path fragment);
    [[nodiscard]] Application build();

private:
    std::unique_ptr<Config> config_;
};

} // namespace mgv
