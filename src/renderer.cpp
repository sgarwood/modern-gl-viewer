#include "mgv/renderer.hpp"

#include "mgv/obj_loader.hpp"
#include "mgv/shader_loader.hpp"

#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <utility>

namespace mgv {
namespace {

[[nodiscard]] Mat4 identity() {
    return {1, 0, 0, 0,
            0, 1, 0, 0,
            0, 0, 1, 0,
            0, 0, 0, 1};
}

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

[[nodiscard]] Mat4 translation(float x, float y, float z) {
    auto result = identity();
    result[12] = x;
    result[13] = y;
    result[14] = z;
    return result;
}

[[nodiscard]] Mat4 uniform_scale(float value) {
    auto result = identity();
    result[0] = value;
    result[5] = value;
    result[10] = value;
    return result;
}

[[nodiscard]] Mat4 model_matrix(float elapsed_seconds, const Vec3& center, float scale) {
    auto rotation = identity();
    const auto sine = std::sin(elapsed_seconds * 0.35F);
    const auto cosine = std::cos(elapsed_seconds * 0.35F);
    rotation[0] = cosine;
    rotation[2] = -sine;
    rotation[8] = sine;
    rotation[10] = cosine;
    return multiply(rotation, multiply(uniform_scale(scale), translation(-center.x, -center.y, -center.z)));
}

struct Placement final {
    Vec3 center;
    float scale{1.0F};
};

[[nodiscard]] Placement fit_to_view(const MeshData& mesh) {
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
    return {.center = center, .scale = extent > 0.0F ? 1.6F / extent : 1.0F};
}

} // namespace

struct Renderer::Impl final {
    explicit Impl(std::unique_ptr<RenderBackend> value) : backend{std::move(value)} {
        if (!backend) {
            throw std::invalid_argument{"Renderer requires a backend"};
        }
    }

    std::unique_ptr<RenderBackend> backend;
    std::unique_ptr<MeshResource> mesh;
    std::unique_ptr<ShaderResource> shader;
    Placement placement;
    Camera camera;
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
    const auto placement = fit_to_view(mesh);
    auto new_mesh = impl_->backend->create_mesh(mesh);
    auto new_shader = impl_->backend->create_shader(shaders);
    if (!new_mesh || !new_shader) {
        throw std::runtime_error{"Render backend returned an empty resource"};
    }
    impl_->mesh = std::move(new_mesh);
    impl_->shader = std::move(new_shader);
    impl_->placement = placement;
}

void Renderer::set_camera(Camera camera) {
    impl_->camera = std::move(camera);
}

Camera Renderer::camera() const {
    return impl_->camera;
}

void Renderer::render(const Frame& frame) {
    if (!impl_->mesh || !impl_->shader) {
        throw std::logic_error{"Renderer assets have not been loaded"};
    }
    const auto aspect = static_cast<float>(frame.framebuffer_width) /
                        static_cast<float>(std::max(frame.framebuffer_height, 1));
    const auto transform = multiply(
        impl_->camera.view_projection_matrix(std::max(aspect, 0.01F)),
        model_matrix(frame.elapsed_seconds, impl_->placement.center, impl_->placement.scale));
    impl_->backend->begin_frame(frame);
    impl_->backend->draw(*impl_->mesh, *impl_->shader, transform);
    impl_->backend->end_frame();
}

} // namespace mgv
