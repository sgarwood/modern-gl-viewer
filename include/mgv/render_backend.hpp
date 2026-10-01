#pragma once

#include "mgv/camera.hpp"
#include "mgv/clip_space.hpp"
#include "mgv/mesh.hpp"
#include "mgv/render_pipeline.hpp"
#include "mgv/texture.hpp"

#include <cstdint>
#include <memory>
#include <span>
#include <string>

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

class TextureResource {
public:
    virtual ~TextureResource() = default;
    TextureResource(const TextureResource&) = delete;
    TextureResource& operator=(const TextureResource&) = delete;

protected:
    TextureResource() = default;
};

class SamplerResource {
public:
    virtual ~SamplerResource() = default;
    SamplerResource(const SamplerResource&) = delete;
    SamplerResource& operator=(const SamplerResource&) = delete;

protected:
    SamplerResource() = default;
};

struct RenderBackendCapabilities final {
    ClipSpaceConvention clip_space;
    bool wireframe{};
    std::uint32_t max_sampled_textures{16};

    friend bool operator==(const RenderBackendCapabilities&, const RenderBackendCapabilities&) = default;
};

struct SampledTextureBinding final {
    std::string name;
    const TextureResource* texture{};
    const SamplerResource* sampler{};
};

struct ColorBinding final {
    std::string name;
    Vec4 value;
};

struct DrawPacket final {
    const MeshResource& mesh;
    const RenderPipelineResource& pipeline;
    Mat4 model_view_projection;
    std::span<const SampledTextureBinding> textures;
    std::span<const ColorBinding> colors;
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
    [[nodiscard]] virtual std::unique_ptr<TextureResource> create_texture(const ImageData& image) = 0;
    [[nodiscard]] virtual std::unique_ptr<SamplerResource> create_sampler(
        const SamplerDescriptor& descriptor) = 0;
    virtual void begin_frame(const Frame& frame) = 0;
    virtual void draw(const DrawPacket& packet) = 0;
    virtual void end_frame() noexcept = 0;

protected:
    RenderBackend() = default;
};

} // namespace mgv
