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

struct GpuRenderable final {
    MeshResource* mesh{};
    ShaderResource* shader{};
    Mat4 model_matrix{};
    bool visible{true};
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
    std::vector<std::unique_ptr<ShaderResource>> shaders;
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
    load(ObjLoader{}.load(paths.model), ShaderLoader::load(paths.vertex_shader, paths.fragment_shader));
}

void Renderer::load(MeshData mesh, ShaderSources shaders) {
    if (mesh.empty()) {
        throw std::invalid_argument{"Cannot create an empty mesh"};
    }
    const auto transform = fit_to_view(mesh);
    auto shared_mesh = std::make_shared<const MeshData>(std::move(mesh));
    auto material = std::make_shared<const Material>(std::move(shaders));
    Scene scene;
    scene.add(Renderable{std::move(shared_mesh), std::move(material), transform});
    set_scene(std::move(scene));
}

void Renderer::set_scene(Scene scene) {
    if (scene.empty()) {
        throw std::invalid_argument{"Cannot render an empty scene"};
    }

    std::vector<std::unique_ptr<MeshResource>> meshes;
    std::vector<std::unique_ptr<ShaderResource>> shaders;
    std::vector<GpuRenderable> renderables;
    std::unordered_map<const MeshData*, MeshResource*> mesh_cache;
    std::unordered_map<const Material*, ShaderResource*> shader_cache;
    renderables.reserve(scene.size());

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

        const auto* shader_key = renderable.material().get();
        auto* gpu_shader = shader_cache.contains(shader_key) ? shader_cache.at(shader_key) : nullptr;
        if (gpu_shader == nullptr) {
            auto resource = impl_->backend->create_shader(renderable.material()->shaders());
            if (!resource) {
                throw std::runtime_error{"Render backend returned an empty shader resource"};
            }
            gpu_shader = resource.get();
            shader_cache.emplace(shader_key, gpu_shader);
            shaders.push_back(std::move(resource));
        }

        renderables.push_back({
            .mesh = gpu_mesh,
            .shader = gpu_shader,
            .model_matrix = renderable.transform().matrix(),
            .visible = renderable.visible(),
        });
    }

    impl_->meshes = std::move(meshes);
    impl_->shaders = std::move(shaders);
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
    const auto view_projection = impl_->camera.view_projection_matrix(std::max(aspect, 0.01F));
    impl_->backend->begin_frame(frame);
    for (const auto& renderable : impl_->renderables) {
        if (renderable.visible) {
            impl_->backend->draw(
                *renderable.mesh,
                *renderable.shader,
                multiply(view_projection, renderable.model_matrix));
        }
    }
    impl_->backend->end_frame();
}

} // namespace mgv
