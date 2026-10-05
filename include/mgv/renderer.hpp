#pragma once

#include "mgv/camera_target.hpp"
#include "mgv/render_backend.hpp"
#include "mgv/scene.hpp"

#include <cstddef>
#include <filesystem>
#include <memory>
#include <vector>

namespace mgv {

struct AssetPaths final {
    std::filesystem::path model;
    std::filesystem::path vertex_shader;
    std::filesystem::path fragment_shader;
};

struct RenderStatistics final {
    std::size_t submitted{};
    std::size_t culled{};
    std::size_t opaque{};
    std::size_t translucent{};

    friend bool operator==(const RenderStatistics&, const RenderStatistics&) = default;
};

class Renderer final : public CameraTarget {
public:
    explicit Renderer(std::unique_ptr<RenderBackend> backend);
    ~Renderer();

    Renderer(Renderer&&) noexcept;
    Renderer& operator=(Renderer&&) noexcept;
    Renderer(const Renderer&) = delete;
    Renderer& operator=(const Renderer&) = delete;

    std::vector<RenderableId> load(const AssetPaths& paths);
    std::vector<RenderableId> load(MeshData mesh, ShaderSources shaders);
    std::vector<RenderableId> set_scene(Scene scene);
    void remove_renderable(RenderableId id);
    void set_renderable_transform(RenderableId id, Transform transform);
    [[nodiscard]] Transform renderable_transform(RenderableId id) const;
    void set_camera(Camera camera) override;
    [[nodiscard]] Camera camera() const override;
    void render(const Frame& frame);
    [[nodiscard]] RenderStatistics last_frame_statistics() const noexcept;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace mgv
