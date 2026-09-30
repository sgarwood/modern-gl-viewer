#pragma once

#include "mgv/camera.hpp"
#include "mgv/mesh.hpp"
#include "mgv/shader_loader.hpp"

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

class ShaderResource {
public:
    virtual ~ShaderResource() = default;
    ShaderResource(const ShaderResource&) = delete;
    ShaderResource& operator=(const ShaderResource&) = delete;

protected:
    ShaderResource() = default;
};

class RenderBackend {
public:
    virtual ~RenderBackend() = default;
    RenderBackend(const RenderBackend&) = delete;
    RenderBackend& operator=(const RenderBackend&) = delete;

    [[nodiscard]] virtual std::unique_ptr<MeshResource> create_mesh(const MeshData& mesh) = 0;
    [[nodiscard]] virtual std::unique_ptr<ShaderResource> create_shader(const ShaderSources& sources) = 0;
    virtual void begin_frame(const Frame& frame) = 0;
    virtual void draw(const MeshResource& mesh, const ShaderResource& shader, const Mat4& model_view_projection) = 0;
    virtual void end_frame() = 0;

protected:
    RenderBackend() = default;
};

} // namespace mgv
