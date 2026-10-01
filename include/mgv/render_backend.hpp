#pragma once

#include "mgv/camera.hpp"
#include "mgv/clip_space.hpp"
#include "mgv/mesh.hpp"
#include "mgv/render_pipeline.hpp"

#include <memory>

namespace mgv {

struct Frame final {
    int framebuffer_width{1};
    int framebuffer_height{1};
    float elapsed_seconds{};
};

class MeshResource {
public:
    virtual ~MeshResource() = default;
    MeshResource(const MeshResource&) = delete;
    MeshResource& operator=(const MeshResource&) = delete;

protected:
    MeshResource() = default;
};

class RenderPipelineResource {
public:
    virtual ~RenderPipelineResource() = default;
    RenderPipelineResource(const RenderPipelineResource&) = delete;
    RenderPipelineResource& operator=(const RenderPipelineResource&) = delete;

protected:
    RenderPipelineResource() = default;
};

struct RenderBackendCapabilities final {
    ClipSpaceConvention clip_space;
    bool wireframe{};

    friend bool operator==(const RenderBackendCapabilities&, const RenderBackendCapabilities&) = default;
};

struct DrawPacket final {
    const MeshResource& mesh;
    const RenderPipelineResource& pipeline;
    Mat4 model_view_projection;
};

class RenderBackend {
public:
    virtual ~RenderBackend() = default;
    RenderBackend(const RenderBackend&) = delete;
    RenderBackend& operator=(const RenderBackend&) = delete;

    [[nodiscard]] virtual RenderBackendCapabilities capabilities() const noexcept = 0;
    [[nodiscard]] virtual std::unique_ptr<MeshResource> create_mesh(const MeshData& mesh) = 0;
    [[nodiscard]] virtual std::unique_ptr<RenderPipelineResource> create_pipeline(
        const RenderPipelineDescriptor& descriptor) = 0;
    virtual void begin_frame(const Frame& frame) = 0;
    virtual void draw(const DrawPacket& packet) = 0;
    virtual void end_frame() noexcept = 0;

protected:
    RenderBackend() = default;
};

} // namespace mgv
