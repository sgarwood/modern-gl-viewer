#pragma once

#include "mgv/render_backend.hpp"

#include <filesystem>
#include <memory>

namespace mgv {

struct AssetPaths final {
    std::filesystem::path model;
    std::filesystem::path vertex_shader;
    std::filesystem::path fragment_shader;
};

class Renderer final {
public:
    explicit Renderer(std::unique_ptr<RenderBackend> backend);
    ~Renderer();

    Renderer(Renderer&&) noexcept;
    Renderer& operator=(Renderer&&) noexcept;
    Renderer(const Renderer&) = delete;
    Renderer& operator=(const Renderer&) = delete;

    void load(const AssetPaths& paths);
    void load(MeshData mesh, ShaderSources shaders);
    void set_camera(Camera camera);
    [[nodiscard]] Camera camera() const;
    void render(const Frame& frame);

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace mgv
