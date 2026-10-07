#pragma once

#include "mgv/camera_target.hpp"
#include "mgv/lighting.hpp"
#include "mgv/render_backend.hpp"
#include "mgv/scene.hpp"

#include <cstddef>
#include <filesystem>
#include <memory>
#include <vector>

namespace mgv {

/// Whether a loaded model keeps its authored units or is rescaled to fill the
/// default view.
///
/// The OBJ viewer wants `fit_to_view` so that any model is immediately
/// visible. A simulation wants `authored`, because its assets are modelled in
/// metres and its physics bodies are sized in metres too.
enum class ModelFit {
    fit_to_view,
    authored,
};

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

    std::vector<RenderableId> load(const AssetPaths& paths, ModelFit fit = ModelFit::fit_to_view);
    std::vector<RenderableId> load(MeshData mesh, ShaderSources shaders);
    std::vector<RenderableId> set_scene(Scene scene);
    void remove_renderable(RenderableId id);
    void set_renderable_transform(RenderableId id, Transform transform);
    /// Replaces the geometry behind a renderable, keeping its material,
    /// transform, and identity. Bounds are recalculated.
    ///
    /// Renderables that were given the same mesh share one backend resource,
    /// so this changes the geometry of every one of them. Returns false if
    /// the backend cannot update geometry in place.
    bool update_mesh(RenderableId id, const MeshData& mesh);
    [[nodiscard]] Transform renderable_transform(RenderableId id) const;
    void set_camera(Camera camera) override;
    [[nodiscard]] Camera camera() const override;
    /// Replaces the sky, sun and fog state shared by every subsequent frame.
    void set_environment(Environment environment);
    [[nodiscard]] Environment environment() const;
    /// Sets the extent of the region the sun's shadow map covers. The centre
    /// is chosen per frame from where the camera is looking.
    void set_shadow_volume(ShadowVolume volume);
    [[nodiscard]] ShadowVolume shadow_volume() const;
    void render(const Frame& frame);
    [[nodiscard]] RenderStatistics last_frame_statistics() const noexcept;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace mgv
