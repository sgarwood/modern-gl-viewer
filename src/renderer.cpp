#include "mgv/renderer.hpp"

#include "mgv/obj_loader.hpp"
#include "mgv/shader_loader.hpp"

#include <algorithm>
#include <stdexcept>
#include <unordered_map>
#include <utility>
#include <vector>

namespace mgv {
namespace {

[[nodiscard]] Mat4 multiply(const Mat4& lhs, const Mat4& rhs) {
    Mat4 result{};
    for (std::size_t column = 0; column < 4; ++column) {
        for (std::size_t row = 0; row < 4; ++row) {
            for (std::size_t inner = 0; inner < 4; ++inner) {
                result[column * 4 + row] += lhs[inner * 4 + row] * rhs[column * 4 + inner];
            }
        }
    }
    return result;
}

[[nodiscard]] Transform fit_to_view(const MeshData& mesh) {
    auto minimum = mesh.vertices.front().position;
    auto maximum = minimum;
    for (const auto& vertex : mesh.vertices) {
        minimum.x = std::min(minimum.x, vertex.position.x);
        minimum.y = std::min(minimum.y, vertex.position.y);
        minimum.z = std::min(minimum.z, vertex.position.z);
        maximum.x = std::max(maximum.x, vertex.position.x);
        maximum.y = std::max(maximum.y, vertex.position.y);
        maximum.z = std::max(maximum.z, vertex.position.z);
    }
    const Vec3 center{
        (minimum.x + maximum.x) * 0.5F,
        (minimum.y + maximum.y) * 0.5F,
        (minimum.z + maximum.z) * 0.5F,
    };
    const auto extent = std::max({maximum.x - minimum.x, maximum.y - minimum.y, maximum.z - minimum.z});
    const auto scale = extent > 0.0F ? 1.6F / extent : 1.0F;
    Transform transform;
    transform.set_uniform_scale(scale).set_position({
        -center.x * scale,
        -center.y * scale,
        -center.z * scale,
    });
    return transform;
}

[[nodiscard]] std::shared_ptr<const Texture> make_checkerboard_texture() {
    constexpr std::uint32_t size = 8;
    std::vector<std::uint8_t> pixels;
    pixels.reserve(static_cast<std::size_t>(size * size * 4));
    for (std::uint32_t y = 0; y < size; ++y) {
        for (std::uint32_t x = 0; x < size; ++x) {
            const auto first = ((x / 2) + (y / 2)) % 2 == 0;
            pixels.push_back(first ? 24 : 20);
            pixels.push_back(first ? 105 : 210);
            pixels.push_back(first ? 210 : 145);
            pixels.push_back(255);
        }
    }
    return std::make_shared<const Texture>(
        ImageData{size, size, PixelFormat::rgba8_unorm, ColorSpace::srgb, std::move(pixels)},
        SamplerDescriptor{.min_filter = TextureFilter::nearest, .mag_filter = TextureFilter::nearest});
}

[[nodiscard]] Scene make_single_renderable_scene(
    MeshData mesh,
    ShaderSources shaders,
    std::shared_ptr<const Texture> texture = nullptr) {
    if (mesh.empty()) {
        throw std::invalid_argument{"Cannot create an empty mesh"};
    }
    const auto transform = fit_to_view(mesh);
    auto shared_mesh = std::make_shared<const MeshData>(std::move(mesh));
    auto material = std::make_shared<const Material>(std::move(shaders));
    auto material_instance = std::make_shared<MaterialInstance>(std::move(material));
    if (texture) {
        material_instance->set_texture("uBaseColorTexture", std::move(texture));
    }
    Scene scene;
    scene.add(Renderable{std::move(shared_mesh), std::move(material_instance), transform});
    return scene;
}

struct GpuRenderable final {
    MeshResource* mesh{};
    RenderPipelineResource* pipeline{};
    Mat4 model_matrix{};
    std::vector<SampledTextureBinding> textures;
    bool visible{true};
};

struct GpuTexture final {
    TextureResource* texture{};
    SamplerResource* sampler{};
};

class FrameScope final {
public:
    FrameScope(RenderBackend& backend, const Frame& frame) : backend_{backend} {
        backend_.begin_frame(frame);
    }
    ~FrameScope() { backend_.end_frame(); }

    FrameScope(const FrameScope&) = delete;
    FrameScope& operator=(const FrameScope&) = delete;

private:
    RenderBackend& backend_;
};

} // namespace

struct Renderer::Impl final {
    explicit Impl(std::unique_ptr<RenderBackend> value) : backend{std::move(value)} {
        if (!backend) {
            throw std::invalid_argument{"Renderer requires a backend"};
        }
    }

    std::unique_ptr<RenderBackend> backend;
    std::vector<std::unique_ptr<MeshResource>> meshes;
    std::vector<std::unique_ptr<RenderPipelineResource>> pipelines;
    std::vector<std::unique_ptr<TextureResource>> textures;
    std::vector<std::unique_ptr<SamplerResource>> samplers;
    std::vector<GpuRenderable> renderables;
    Camera camera;
    bool scene_loaded{};
};

Renderer::Renderer(std::unique_ptr<RenderBackend> backend)
    : impl_{std::make_unique<Impl>(std::move(backend))} {}
Renderer::~Renderer() = default;
Renderer::Renderer(Renderer&&) noexcept = default;
Renderer& Renderer::operator=(Renderer&&) noexcept = default;

void Renderer::load(const AssetPaths& paths) {
    set_scene(make_single_renderable_scene(
        ObjLoader{}.load(paths.model),
        ShaderLoader::load(paths.vertex_shader, paths.fragment_shader),
        make_checkerboard_texture()));
}

void Renderer::load(MeshData mesh, ShaderSources shaders) {
    set_scene(make_single_renderable_scene(std::move(mesh), std::move(shaders)));
}

void Renderer::set_scene(Scene scene) {
    if (scene.empty()) {
        throw std::invalid_argument{"Cannot render an empty scene"};
    }

    std::vector<std::unique_ptr<MeshResource>> meshes;
    std::vector<std::unique_ptr<RenderPipelineResource>> pipelines;
    std::vector<std::unique_ptr<TextureResource>> textures;
    std::vector<std::unique_ptr<SamplerResource>> samplers;
    std::vector<GpuRenderable> renderables;
    std::unordered_map<const MeshData*, MeshResource*> mesh_cache;
    std::unordered_map<const Material*, RenderPipelineResource*> pipeline_cache;
    std::unordered_map<const Texture*, GpuTexture> texture_cache;
    renderables.reserve(scene.size());

    const auto capabilities = impl_->backend->capabilities();

    for (const auto& renderable : scene.renderables()) {
        const auto* mesh_key = renderable.mesh().get();
        auto* gpu_mesh = mesh_cache.contains(mesh_key) ? mesh_cache.at(mesh_key) : nullptr;
        if (gpu_mesh == nullptr) {
            auto resource = impl_->backend->create_mesh(*renderable.mesh());
            if (!resource) {
                throw std::runtime_error{"Render backend returned an empty mesh resource"};
            }
            gpu_mesh = resource.get();
            mesh_cache.emplace(mesh_key, gpu_mesh);
            meshes.push_back(std::move(resource));
        }

        const auto* pipeline_key = renderable.material_instance()->material().get();
        auto* gpu_pipeline = pipeline_cache.contains(pipeline_key) ? pipeline_cache.at(pipeline_key) : nullptr;
        if (gpu_pipeline == nullptr) {
            auto resource = impl_->backend->create_pipeline(renderable.material_instance()->material()->pipeline());
            if (!resource) {
                throw std::runtime_error{"Render backend returned an empty pipeline resource"};
            }
            gpu_pipeline = resource.get();
            pipeline_cache.emplace(pipeline_key, gpu_pipeline);
            pipelines.push_back(std::move(resource));
        }

        if (renderable.material_instance()->texture_bindings().size() > capabilities.max_sampled_textures) {
            throw std::runtime_error{"Material exceeds the backend sampled-texture limit"};
        }
        std::vector<SampledTextureBinding> gpu_texture_bindings;
        gpu_texture_bindings.reserve(renderable.material_instance()->texture_bindings().size());
        for (const auto& binding : renderable.material_instance()->texture_bindings()) {
            const auto* texture_key = binding.texture.get();
            auto gpu_texture = texture_cache.contains(texture_key) ? texture_cache.at(texture_key) : GpuTexture{};
            if (gpu_texture.texture == nullptr) {
                auto texture_resource = impl_->backend->create_texture(binding.texture->image());
                if (!texture_resource) {
                    throw std::runtime_error{"Render backend returned an empty texture resource"};
                }
                auto sampler_resource = impl_->backend->create_sampler(binding.texture->sampler());
                if (!sampler_resource) {
                    throw std::runtime_error{"Render backend returned an empty sampler resource"};
                }
                gpu_texture = {
                    .texture = texture_resource.get(),
                    .sampler = sampler_resource.get(),
                };
                texture_cache.emplace(texture_key, gpu_texture);
                textures.push_back(std::move(texture_resource));
                samplers.push_back(std::move(sampler_resource));
            }
            gpu_texture_bindings.push_back({
                .name = binding.name,
                .texture = gpu_texture.texture,
                .sampler = gpu_texture.sampler,
            });
        }

        renderables.push_back({
            .mesh = gpu_mesh,
            .pipeline = gpu_pipeline,
            .model_matrix = renderable.transform().matrix(),
            .textures = std::move(gpu_texture_bindings),
            .visible = renderable.visible(),
        });
    }

    impl_->meshes = std::move(meshes);
    impl_->pipelines = std::move(pipelines);
    impl_->textures = std::move(textures);
    impl_->samplers = std::move(samplers);
    impl_->renderables = std::move(renderables);
    impl_->scene_loaded = true;
}

void Renderer::set_camera(Camera camera) {
    impl_->camera = std::move(camera);
}

Camera Renderer::camera() const {
    return impl_->camera;
}

void Renderer::render(const Frame& frame) {
    if (!impl_->scene_loaded) {
        throw std::logic_error{"Renderer assets have not been loaded"};
    }
    const auto aspect = static_cast<float>(frame.framebuffer_width) /
                        static_cast<float>(std::max(frame.framebuffer_height, 1));
    const auto capabilities = impl_->backend->capabilities();
    const auto view_projection = impl_->camera.view_projection_matrix(
        std::max(aspect, 0.01F),
        capabilities.clip_space);
    const FrameScope frame_scope{*impl_->backend, frame};
    for (const auto& renderable : impl_->renderables) {
        if (renderable.visible) {
            impl_->backend->draw({
                .mesh = *renderable.mesh,
                .pipeline = *renderable.pipeline,
                .model_view_projection = multiply(view_projection, renderable.model_matrix),
                .textures = renderable.textures,
            });
        }
    }
}

} // namespace mgv
