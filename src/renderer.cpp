#include "mgv/renderer.hpp"

#include "mgv/bounds.hpp"
#include "mgv/image_loader.hpp"
#include "mgv/obj_loader.hpp"
#include "mgv/shader_loader.hpp"

#include <algorithm>
#include <cmath>
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

/// Returns the inverse transpose of the upper-left 3x3 block of `model`, so
/// normals survive non-uniform scale. Translation is dropped and a singular
/// basis degrades to the identity rather than producing infinities.
[[nodiscard]] Mat4 normal_matrix_of(const Mat4& model) {
    const auto m = [&model](std::size_t column, std::size_t row) {
        return model[column * 4 + row];
    };
    const auto cofactor00 = m(1, 1) * m(2, 2) - m(2, 1) * m(1, 2);
    const auto cofactor01 = m(2, 1) * m(0, 2) - m(0, 1) * m(2, 2);
    const auto cofactor02 = m(0, 1) * m(1, 2) - m(1, 1) * m(0, 2);
    const auto determinant = m(0, 0) * cofactor00 + m(1, 0) * cofactor01 + m(2, 0) * cofactor02;
    if (std::abs(determinant) < 1.0e-12F) {
        return identity_matrix;
    }
    const auto inverse_determinant = 1.0F / determinant;

    // The inverse of the 3x3 basis is the adjugate over the determinant; the
    // transpose of that inverse is the adjugate's transpose, which is the
    // cofactor matrix laid out column by column.
    Mat4 result{identity_matrix};
    const auto set = [&result](std::size_t column, std::size_t row, float value) {
        result[column * 4 + row] = value;
    };
    set(0, 0, cofactor00 * inverse_determinant);
    set(0, 1, cofactor01 * inverse_determinant);
    set(0, 2, cofactor02 * inverse_determinant);
    set(1, 0, (m(2, 0) * m(1, 2) - m(1, 0) * m(2, 2)) * inverse_determinant);
    set(1, 1, (m(0, 0) * m(2, 2) - m(2, 0) * m(0, 2)) * inverse_determinant);
    set(1, 2, (m(1, 0) * m(0, 2) - m(0, 0) * m(1, 2)) * inverse_determinant);
    set(2, 0, (m(1, 0) * m(2, 1) - m(2, 0) * m(1, 1)) * inverse_determinant);
    set(2, 1, (m(2, 0) * m(0, 1) - m(0, 0) * m(2, 1)) * inverse_determinant);
    set(2, 2, (m(0, 0) * m(1, 1) - m(1, 0) * m(0, 1)) * inverse_determinant);
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

[[nodiscard]] std::shared_ptr<const Texture> make_white_texture() {
    return std::make_shared<const Texture>(ImageData{
        1,
        1,
        PixelFormat::rgba8_unorm,
        ColorSpace::srgb,
        {255, 255, 255, 255},
    });
}

[[nodiscard]] Transform fit_to_view(const ImportedModel& model) {
    auto minimum = model.primitives.front().mesh.vertices.front().position;
    auto maximum = minimum;
    for (const auto& primitive : model.primitives) {
        for (const auto& vertex : primitive.mesh.vertices) {
            minimum.x = std::min(minimum.x, vertex.position.x);
            minimum.y = std::min(minimum.y, vertex.position.y);
            minimum.z = std::min(minimum.z, vertex.position.z);
            maximum.x = std::max(maximum.x, vertex.position.x);
            maximum.y = std::max(maximum.y, vertex.position.y);
            maximum.z = std::max(maximum.z, vertex.position.z);
        }
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

[[nodiscard]] Scene make_imported_scene(
    ImportedModel model,
    ShaderSources shaders,
    ModelFit fit) {
    if (model.primitives.empty()) {
        throw std::invalid_argument{"Cannot create an empty imported model"};
    }
    const auto transform = fit == ModelFit::authored ? Transform{} : fit_to_view(model);
    const auto material = std::make_shared<const Material>(std::move(shaders));
    auto transparent_pipeline = material->pipeline();
    transparent_pipeline.depth.write_enabled = false;
    transparent_pipeline.blending.enabled = true;
    const auto transparent_material =
        std::make_shared<const Material>(std::move(transparent_pipeline));
    const auto white = make_white_texture();
    std::unordered_map<std::filesystem::path, std::shared_ptr<const Texture>> texture_cache;
    std::vector<std::shared_ptr<const MaterialInstance>> material_instances;
    material_instances.reserve(model.materials.size());
    for (const auto& imported : model.materials) {
        auto instance = std::make_shared<MaterialInstance>(
            imported.opacity < 1.0F ? transparent_material : material);
        instance->set_color("uBaseColorFactor", {
            imported.diffuse_color.x,
            imported.diffuse_color.y,
            imported.diffuse_color.z,
            imported.opacity,
        });
        std::shared_ptr<const Texture> texture = white;
        if (imported.diffuse_texture) {
            const auto path = imported.diffuse_texture->lexically_normal();
            const auto existing = texture_cache.find(path);
            if (existing != texture_cache.end()) {
                texture = existing->second;
            } else {
                texture = std::make_shared<const Texture>(ImageLoader{}.load(path, ColorSpace::srgb));
                texture_cache.emplace(path, texture);
            }
        }
        instance->set_texture("uBaseColorTexture", std::move(texture));
        material_instances.push_back(std::move(instance));
    }
    auto fallback = std::make_shared<MaterialInstance>(material);
    fallback->set_color("uBaseColorFactor", {1.0F, 1.0F, 1.0F, 1.0F});
    fallback->set_texture("uBaseColorTexture", make_checkerboard_texture());

    Scene scene;
    for (auto& primitive : model.primitives) {
        const auto instance = primitive.material_index
            ? material_instances.at(*primitive.material_index)
            : std::shared_ptr<const MaterialInstance>{fallback};
        scene.add(Renderable{
            std::make_shared<const MeshData>(std::move(primitive.mesh)),
            instance,
            transform,
        });
    }
    return scene;
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
    RenderableId id;
    MeshResource* mesh{};
    RenderPipelineResource* pipeline{};
    Transform transform;
    Mat4 model_matrix{};
    Mat4 normal_matrix{};
    std::vector<SampledTextureBinding> textures;
    std::vector<ColorBinding> colors;
    BoundingSphere local_bounds;
    bool translucent{};
    bool visible{true};
};

struct QueuedRenderable final {
    const GpuRenderable* renderable{};
    float distance_squared{};
};

struct GpuMesh final {
    MeshResource* resource{};
    BoundingSphere bounds;
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
    std::vector<QueuedRenderable> queue;
    Camera camera;
    Environment environment;
    RenderStatistics statistics;
    bool scene_loaded{};
};

Renderer::Renderer(std::unique_ptr<RenderBackend> backend)
    : impl_{std::make_unique<Impl>(std::move(backend))} {}
Renderer::~Renderer() = default;
Renderer::Renderer(Renderer&&) noexcept = default;
Renderer& Renderer::operator=(Renderer&&) noexcept = default;

std::vector<RenderableId> Renderer::load(const AssetPaths& paths, ModelFit fit) {
    return set_scene(make_imported_scene(
        ObjLoader{}.load_model(paths.model),
        ShaderLoader::load(paths.vertex_shader, paths.fragment_shader),
        fit));
}

std::vector<RenderableId> Renderer::load(MeshData mesh, ShaderSources shaders) {
    return set_scene(make_single_renderable_scene(std::move(mesh), std::move(shaders)));
}

std::vector<RenderableId> Renderer::set_scene(Scene scene) {
    if (scene.empty()) {
        throw std::invalid_argument{"Cannot render an empty scene"};
    }

    std::vector<std::unique_ptr<MeshResource>> meshes;
    std::vector<std::unique_ptr<RenderPipelineResource>> pipelines;
    std::vector<std::unique_ptr<TextureResource>> textures;
    std::vector<std::unique_ptr<SamplerResource>> samplers;
    std::vector<GpuRenderable> renderables;
    std::unordered_map<const MeshData*, GpuMesh> mesh_cache;
    std::unordered_map<const Material*, RenderPipelineResource*> pipeline_cache;
    std::unordered_map<const Texture*, GpuTexture> texture_cache;
    renderables.reserve(scene.size());

    const auto capabilities = impl_->backend->capabilities();

    for (std::size_t index = 0; index < scene.size(); ++index) {
        const auto& renderable = scene.renderables()[index];
        const auto* mesh_key = renderable.mesh().get();
        auto gpu_mesh = mesh_cache.contains(mesh_key) ? mesh_cache.at(mesh_key) : GpuMesh{};
        if (gpu_mesh.resource == nullptr) {
            auto resource = impl_->backend->create_mesh(*renderable.mesh());
            if (!resource) {
                throw std::runtime_error{"Render backend returned an empty mesh resource"};
            }
            gpu_mesh = {
                .resource = resource.get(),
                .bounds = calculate_bounds(*renderable.mesh()).sphere,
            };
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

        std::vector<ColorBinding> gpu_color_bindings;
        gpu_color_bindings.reserve(renderable.material_instance()->color_bindings().size());
        for (const auto& binding : renderable.material_instance()->color_bindings()) {
            gpu_color_bindings.push_back({.name = binding.name, .value = binding.value});
        }

        renderables.push_back({
            .id = scene.renderable_ids()[index],
            .mesh = gpu_mesh.resource,
            .pipeline = gpu_pipeline,
            .transform = renderable.transform(),
            .model_matrix = renderable.transform().matrix(),
            .normal_matrix = normal_matrix_of(renderable.transform().matrix()),
            .textures = std::move(gpu_texture_bindings),
            .colors = std::move(gpu_color_bindings),
            .local_bounds = gpu_mesh.bounds,
            .translucent = renderable.material_instance()->material()->pipeline().blending.enabled,
            .visible = renderable.visible(),
        });
    }

    impl_->meshes = std::move(meshes);
    impl_->pipelines = std::move(pipelines);
    impl_->textures = std::move(textures);
    impl_->samplers = std::move(samplers);
    impl_->renderables = std::move(renderables);
    impl_->queue.clear();
    impl_->queue.reserve(impl_->renderables.size());
    impl_->statistics = {};
    impl_->scene_loaded = true;

    std::vector<RenderableId> ids;
    ids.reserve(impl_->renderables.size());
    for (const auto& renderable : impl_->renderables) {
        ids.push_back(renderable.id);
    }
    return ids;
}

void Renderer::set_camera(Camera camera) {
    impl_->camera = std::move(camera);
}

void Renderer::remove_renderable(RenderableId id) {
    const auto found = std::ranges::find(impl_->renderables, id, &GpuRenderable::id);
    if (found == impl_->renderables.end()) {
        throw std::out_of_range{"Unknown renderable identifier"};
    }
    impl_->renderables.erase(found);
    impl_->queue.clear();
}

void Renderer::set_renderable_transform(RenderableId id, Transform transform) {
    const auto found = std::ranges::find(impl_->renderables, id, &GpuRenderable::id);
    if (found == impl_->renderables.end()) {
        throw std::out_of_range{"Unknown renderable identifier"};
    }
    found->model_matrix = transform.matrix();
    found->normal_matrix = normal_matrix_of(found->model_matrix);
    found->transform = std::move(transform);
}

Transform Renderer::renderable_transform(RenderableId id) const {
    const auto found = std::ranges::find(impl_->renderables, id, &GpuRenderable::id);
    if (found == impl_->renderables.end()) {
        throw std::out_of_range{"Unknown renderable identifier"};
    }
    return found->transform;
}

Camera Renderer::camera() const {
    return impl_->camera;
}

void Renderer::set_environment(Environment environment) {
    environment.sun.direction = normalized_light_direction(environment.sun.direction);
    impl_->environment = environment;
}

Environment Renderer::environment() const {
    return impl_->environment;
}

void Renderer::render(const Frame& frame) {
    if (!impl_->scene_loaded) {
        throw std::logic_error{"Renderer assets have not been loaded"};
    }
    const auto aspect = static_cast<float>(frame.framebuffer_width) /
                        static_cast<float>(std::max(frame.framebuffer_height, 1));
    const auto capabilities = impl_->backend->capabilities();
    const auto safe_aspect = std::max(aspect, 0.01F);
    const auto view = impl_->camera.view_matrix();
    const auto projection = impl_->camera.projection_matrix(safe_aspect, capabilities.clip_space);
    const auto view_projection = impl_->camera.view_projection_matrix(
        safe_aspect,
        capabilities.clip_space);
    const auto frustum = Frustum::from_view_projection(view_projection, capabilities.clip_space);
    impl_->queue.clear();
    RenderStatistics statistics;
    for (const auto& renderable : impl_->renderables) {
        if (!renderable.visible) {
            continue;
        }
        const auto world_bounds = transform_bounds(renderable.local_bounds, renderable.model_matrix);
        if (!frustum.intersects(world_bounds)) {
            ++statistics.culled;
            continue;
        }
        const auto delta_x = world_bounds.center.x - impl_->camera.position().x;
        const auto delta_y = world_bounds.center.y - impl_->camera.position().y;
        const auto delta_z = world_bounds.center.z - impl_->camera.position().z;
        impl_->queue.push_back({
            .renderable = &renderable,
            .distance_squared = delta_x * delta_x + delta_y * delta_y + delta_z * delta_z,
        });
        if (renderable.translucent) {
            ++statistics.translucent;
        } else {
            ++statistics.opaque;
        }
    }
    std::stable_sort(impl_->queue.begin(), impl_->queue.end(), [](const auto& lhs, const auto& rhs) {
        if (lhs.renderable->translucent != rhs.renderable->translucent) {
            return !lhs.renderable->translucent;
        }
        return lhs.renderable->translucent
            ? lhs.distance_squared > rhs.distance_squared
            : lhs.distance_squared < rhs.distance_squared;
    });
    statistics.submitted = impl_->queue.size();
    impl_->statistics = statistics;
    auto resolved = frame;
    resolved.view = view;
    resolved.projection = projection;
    resolved.view_projection = view_projection;
    resolved.camera_position = impl_->camera.position();
    resolved.environment = impl_->environment;

    const FrameScope frame_scope{*impl_->backend, resolved};
    for (const auto& item : impl_->queue) {
        const auto& renderable = *item.renderable;
        impl_->backend->draw({
            .mesh = *renderable.mesh,
            .pipeline = *renderable.pipeline,
            .model_view_projection = multiply(view_projection, renderable.model_matrix),
            .model = renderable.model_matrix,
            .normal_matrix = renderable.normal_matrix,
            .textures = renderable.textures,
            .colors = renderable.colors,
        });
    }
}

RenderStatistics Renderer::last_frame_statistics() const noexcept {
    return impl_->statistics;
}

} // namespace mgv
