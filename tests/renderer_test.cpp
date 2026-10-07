#include "mgv/renderer.hpp"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <cstddef>
#include <filesystem>
#include <memory>
#include <stdexcept>
#include <vector>

namespace {

struct Calls final {
    int meshes{};
    int pipelines{};
    int textures{};
    int samplers{};
    int begins{};
    int draws{};
    int ends{};
    bool reject_pipelines{};
    bool reject_draws{};
    bool reject_samplers{};
    bool refuse_mesh_updates{};
    int mesh_updates{};
    std::size_t last_updated_vertices{};
    mgv::PrimitiveTopology topology{mgv::PrimitiveTopology::triangle_list};
    mgv::CullMode cull_mode{mgv::CullMode::none};
    mgv::PolygonMode polygon_mode{mgv::PolygonMode::fill};
    mgv::CompareOperation depth_compare{mgv::CompareOperation::less};
    bool depth_write{};
    bool blending{};
    std::vector<std::string> texture_binding_names;
    std::vector<mgv::Vec4> colors;
    std::vector<float> draw_markers;
    std::vector<mgv::Mat4> transforms;
    std::vector<mgv::Mat4> models;
    std::vector<mgv::Mat4> normal_matrices;
    std::vector<mgv::Mat4> frame_views;
    std::vector<mgv::Mat4> frame_projections;
    std::vector<mgv::Mat4> frame_view_projections;
    std::vector<mgv::Vec3> frame_camera_positions;
    std::vector<mgv::Environment> frame_environments;
    std::vector<std::size_t> frame_trail_sizes;
};

[[nodiscard]] mgv::MeshData unit_triangle() {
    return {
        .vertices = {{{0, 0, 0}, {0, 0, 1}, {0, 0}},
                     {{1, 0, 0}, {0, 0, 1}, {1, 0}},
                     {{0, 1, 0}, {0, 0, 1}, {0, 1}}},
        .indices = {0, 1, 2},
    };
}

class FakeMesh final : public mgv::MeshResource {};
class FakePipeline final : public mgv::RenderPipelineResource {};
class FakeTexture final : public mgv::TextureResource {};
class FakeSampler final : public mgv::SamplerResource {};

class FakeBackend final : public mgv::RenderBackend {
public:
    explicit FakeBackend(Calls& calls, mgv::RenderBackendCapabilities capabilities = {})
        : calls_{calls}, capabilities_{capabilities} {}

    [[nodiscard]] mgv::RenderBackendCapabilities capabilities() const noexcept override {
        return capabilities_;
    }

    std::unique_ptr<mgv::MeshResource> create_mesh(const mgv::MeshData&) override {
        ++calls_.meshes;
        return std::make_unique<FakeMesh>();
    }
    bool update_mesh(mgv::MeshResource&, const mgv::MeshData& mesh) override {
        if (calls_.refuse_mesh_updates) {
            return false;
        }
        ++calls_.mesh_updates;
        calls_.last_updated_vertices = mesh.vertices.size();
        return true;
    }
    std::unique_ptr<mgv::RenderPipelineResource> create_pipeline(
        const mgv::RenderPipelineDescriptor& descriptor) override {
        ++calls_.pipelines;
        calls_.topology = descriptor.topology;
        calls_.cull_mode = descriptor.rasterization.cull_mode;
        calls_.polygon_mode = descriptor.rasterization.polygon_mode;
        calls_.depth_compare = descriptor.depth.compare;
        calls_.depth_write = descriptor.depth.write_enabled;
        calls_.blending = descriptor.blending.enabled;
        if (calls_.reject_pipelines) {
            return nullptr;
        }
        return std::make_unique<FakePipeline>();
    }
    std::unique_ptr<mgv::TextureResource> create_texture(const mgv::ImageData&) override {
        ++calls_.textures;
        return std::make_unique<FakeTexture>();
    }
    std::unique_ptr<mgv::SamplerResource> create_sampler(const mgv::SamplerDescriptor&) override {
        ++calls_.samplers;
        if (calls_.reject_samplers) {
            return nullptr;
        }
        return std::make_unique<FakeSampler>();
    }
    void begin_frame(const mgv::Frame& frame) override {
        ++calls_.begins;
        calls_.frame_views.push_back(frame.view);
        calls_.frame_projections.push_back(frame.projection);
        calls_.frame_view_projections.push_back(frame.view_projection);
        calls_.frame_camera_positions.push_back(frame.camera_position);
        calls_.frame_environments.push_back(frame.environment);
        calls_.frame_trail_sizes.push_back(frame.wet_trail.size());
    }
    void draw(const mgv::DrawPacket& packet) override {
        ++calls_.draws;
        calls_.transforms.push_back(packet.model_view_projection);
        calls_.models.push_back(packet.model);
        calls_.normal_matrices.push_back(packet.normal_matrix);
        for (const auto& binding : packet.textures) {
            REQUIRE(binding.texture != nullptr);
            REQUIRE(binding.sampler != nullptr);
            calls_.texture_binding_names.push_back(binding.name);
        }
        for (const auto& binding : packet.colors) {
            calls_.colors.push_back(binding.value);
        }
        if (!packet.colors.empty()) {
            calls_.draw_markers.push_back(packet.colors.front().value.x);
        }
        if (calls_.reject_draws) {
            throw std::runtime_error{"draw rejected"};
        }
    }
    void end_frame() noexcept override { ++calls_.ends; }

private:
    Calls& calls_;
    mgv::RenderBackendCapabilities capabilities_;
};

} // namespace

TEST_CASE("renderer facade creates backend resources and submits a frame") {
    Calls calls;
    mgv::Renderer renderer{std::make_unique<FakeBackend>(calls)};
    mgv::MeshData mesh{
        .vertices = {{{0, 0, 0}, {0, 0, 1}, {0, 0}},
                     {{1, 0, 0}, {0, 0, 1}, {1, 0}},
                     {{0, 1, 0}, {0, 0, 1}, {0, 1}}},
        .indices = {0, 1, 2},
    };
    renderer.load(std::move(mesh), {"vertex", "fragment", "test.vert", "test.frag"});

    renderer.render({.framebuffer_width = 800, .framebuffer_height = 600, .elapsed_seconds = 1.0F});

    CHECK(calls.meshes == 1);
    CHECK(calls.pipelines == 1);
    CHECK(calls.topology == mgv::PrimitiveTopology::triangle_list);
    CHECK(calls.cull_mode == mgv::CullMode::none);
    CHECK(calls.polygon_mode == mgv::PolygonMode::fill);
    CHECK(calls.depth_write);
    CHECK_FALSE(calls.blending);
    CHECK(calls.begins == 1);
    CHECK(calls.draws == 1);
    CHECK(calls.ends == 1);
}

TEST_CASE("renderer imports OBJ material primitives and reuses shared images") {
    Calls calls;
    mgv::Renderer renderer{std::make_unique<FakeBackend>(calls)};
    const auto fixture_root = std::filesystem::path{MGV_TEST_FIXTURES};

    renderer.load({
        .model = fixture_root / "material_model" / "model.obj",
        .vertex_shader = fixture_root / "basic.vert",
        .fragment_shader = fixture_root / "basic.frag",
    });
    renderer.render({});

    CHECK(calls.meshes == 2);
    CHECK(calls.pipelines == 2);
    CHECK(calls.textures == 1);
    CHECK(calls.samplers == 1);
    CHECK(calls.draws == 2);
    CHECK_FALSE(calls.depth_write);
    CHECK(calls.blending);
    CHECK(calls.colors == std::vector<mgv::Vec4>{{0.8F, 0.2F, 0.1F, 1.0F},
                                                {0.1F, 0.3F, 0.8F, 0.5F}});
}

TEST_CASE("failed asset import leaves the previous renderer scene intact") {
    Calls calls;
    mgv::Renderer renderer{std::make_unique<FakeBackend>(calls)};
    mgv::MeshData mesh{
        .vertices = {{{0, 0, 0}, {0, 0, 1}, {0, 0}},
                     {{1, 0, 0}, {0, 0, 1}, {1, 0}},
                     {{0, 1, 0}, {0, 0, 1}, {0, 1}}},
        .indices = {0, 1, 2},
    };
    renderer.load(std::move(mesh), {"vertex", "fragment", "test.vert", "test.frag"});
    const auto fixture_root = std::filesystem::path{MGV_TEST_FIXTURES};

    CHECK_THROWS(renderer.load({
        .model = fixture_root / "missing_texture" / "model.obj",
        .vertex_shader = fixture_root / "basic.vert",
        .fragment_shader = fixture_root / "basic.frag",
    }));
    renderer.render({});

    CHECK(calls.draws == 1);
}

TEST_CASE("renderer facade refuses to render before assets are loaded") {
    Calls calls;
    mgv::Renderer renderer{std::make_unique<FakeBackend>(calls)};

    CHECK_THROWS(renderer.render({}));
    CHECK(calls.begins == 0);
}

TEST_CASE("renderer facade requires a backend") {
    CHECK_THROWS_AS(mgv::Renderer{nullptr}, std::invalid_argument);
}

TEST_CASE("renderer facade rejects empty geometry before touching the backend") {
    Calls calls;
    mgv::Renderer renderer{std::make_unique<FakeBackend>(calls)};

    CHECK_THROWS_AS(
        renderer.load({}, {"vertex", "fragment", "test.vert", "test.frag"}),
        std::invalid_argument);
    CHECK(calls.meshes == 0);
    CHECK(calls.pipelines == 0);
}

TEST_CASE("renderer facade owns a configurable camera") {
    Calls calls;
    mgv::Renderer renderer{std::make_unique<FakeBackend>(calls)};
    mgv::Camera camera;
    camera.look_at({4.0F, 3.0F, 6.0F}, {0.0F, 0.0F, 0.0F});

    renderer.set_camera(camera);

    CHECK(renderer.camera().position() == mgv::Vec3{4.0F, 3.0F, 6.0F});
    CHECK(renderer.camera().target() == mgv::Vec3{0.0F, 0.0F, 0.0F});
}

TEST_CASE("renderer submits every visible scene object and reuses shared GPU resources") {
    Calls calls;
    mgv::Renderer renderer{std::make_unique<FakeBackend>(calls)};
    const auto mesh = std::make_shared<const mgv::MeshData>(mgv::MeshData{
        .vertices = {{{0, 0, 0}, {0, 0, 1}, {0, 0}},
                     {{1, 0, 0}, {0, 0, 1}, {1, 0}},
                     {{0, 1, 0}, {0, 0, 1}, {0, 1}}},
        .indices = {0, 1, 2},
    });
    const auto material = std::make_shared<const mgv::Material>(
        mgv::ShaderSources{"vertex", "fragment", "test.vert", "test.frag"});
    const auto material_instance = std::make_shared<const mgv::MaterialInstance>(material);
    mgv::Transform moved;
    moved.set_position({0.5F, 0.0F, 0.0F});
    mgv::Scene scene;
    scene.add(mgv::Renderable{mesh, material_instance});
    scene.add(mgv::Renderable{mesh, material_instance, moved});
    scene.add(mgv::Renderable{mesh, material_instance}.set_visible(false));

    renderer.set_scene(std::move(scene));
    renderer.render({.framebuffer_width = 800, .framebuffer_height = 600});

    CHECK(calls.meshes == 1);
    CHECK(calls.pipelines == 1);
    CHECK(calls.begins == 1);
    CHECK(calls.draws == 2);
    CHECK(calls.ends == 1);
    REQUIRE(calls.transforms.size() == 2);
    CHECK(calls.transforms[0] != calls.transforms[1]);
    CHECK(renderer.last_frame_statistics() == mgv::RenderStatistics{
                                                   .submitted = 2,
                                                   .culled = 0,
                                                   .opaque = 2,
                                                   .translucent = 0,
                                               });
}

TEST_CASE("renderer culls objects outside the camera frustum") {
    Calls calls;
    mgv::Renderer renderer{std::make_unique<FakeBackend>(calls)};
    const auto mesh = std::make_shared<const mgv::MeshData>(mgv::MeshData{
        .vertices = {{{-0.5F, -0.5F, 0.0F}, {}, {}},
                     {{0.5F, -0.5F, 0.0F}, {}, {}},
                     {{0.0F, 0.5F, 0.0F}, {}, {}}},
        .indices = {0, 1, 2},
    });
    const auto instance = std::make_shared<const mgv::MaterialInstance>(
        std::make_shared<const mgv::Material>(
            mgv::ShaderSources{"vertex", "fragment", "test.vert", "test.frag"}));
    mgv::Transform outside;
    outside.set_position({100.0F, 0.0F, 0.0F});
    mgv::Scene scene;
    scene.add(mgv::Renderable{mesh, instance});
    scene.add(mgv::Renderable{mesh, instance, outside});

    renderer.set_scene(std::move(scene));
    renderer.render({.framebuffer_width = 800, .framebuffer_height = 600});

    CHECK(calls.draws == 1);
    CHECK(renderer.last_frame_statistics() == mgv::RenderStatistics{
                                                   .submitted = 1,
                                                   .culled = 1,
                                                   .opaque = 1,
                                                   .translucent = 0,
                                               });
}

TEST_CASE("renderer updates a transform without recreating backend resources") {
    Calls calls;
    mgv::Renderer renderer{std::make_unique<FakeBackend>(calls)};
    mgv::MeshData mesh{
        .vertices = {{{-0.5F, -0.5F, 0.0F}, {}, {}},
                     {{0.5F, -0.5F, 0.0F}, {}, {}},
                     {{0.0F, 0.5F, 0.0F}, {}, {}}},
        .indices = {0, 1, 2},
    };
    const auto renderables = renderer.load(
        std::move(mesh), {"vertex", "fragment", "test.vert", "test.frag"});
    REQUIRE(renderables.size() == 1);
    mgv::Transform outside;
    outside.set_position({100.0F, 0.0F, 0.0F});

    renderer.set_renderable_transform(renderables.front(), outside);
    renderer.render({});

    CHECK(calls.meshes == 1);
    CHECK(calls.pipelines == 1);
    CHECK(calls.draws == 0);
    CHECK(renderer.last_frame_statistics().culled == 1);
    CHECK_THROWS_AS(
        renderer.set_renderable_transform(mgv::RenderableId{999'999}, {}),
        std::out_of_range);
}

TEST_CASE("renderer handles survive erasure without index aliasing") {
    Calls calls;
    mgv::Renderer renderer{std::make_unique<FakeBackend>(calls)};
    const auto mesh = std::make_shared<const mgv::MeshData>(mgv::MeshData{
        .vertices = {{{-0.5F, -0.5F, 0.0F}, {}, {}},
                     {{0.5F, -0.5F, 0.0F}, {}, {}},
                     {{0.0F, 0.5F, 0.0F}, {}, {}}},
        .indices = {0, 1, 2},
    });
    const auto material = std::make_shared<const mgv::MaterialInstance>(
        std::make_shared<const mgv::Material>(
            mgv::ShaderSources{"vertex", "fragment", "test.vert", "test.frag"}));
    mgv::Scene scene;
    const auto removed = scene.add(mgv::Renderable{mesh, material});
    const auto retained = scene.add(mgv::Renderable{mesh, material});
    renderer.set_scene(std::move(scene));

    renderer.remove_renderable(removed);
    mgv::Transform moved;
    moved.set_position({1.0F, 0.0F, 0.0F});
    renderer.set_renderable_transform(retained, moved);
    renderer.render({});

    CHECK(calls.draws == 1);
    CHECK_THROWS_AS(renderer.set_renderable_transform(removed, {}), std::out_of_range);
}

TEST_CASE("renderer queues opaque front-to-back before translucent back-to-front") {
    Calls calls;
    mgv::Renderer renderer{std::make_unique<FakeBackend>(calls)};
    const auto mesh = std::make_shared<const mgv::MeshData>(mgv::MeshData{
        .vertices = {{{-0.1F, -0.1F, 0.0F}, {}, {}},
                     {{0.1F, -0.1F, 0.0F}, {}, {}},
                     {{0.0F, 0.1F, 0.0F}, {}, {}}},
        .indices = {0, 1, 2},
    });
    const auto opaque_material = std::make_shared<const mgv::Material>(
        mgv::ShaderSources{"vertex", "fragment", "opaque.vert", "opaque.frag"});
    auto translucent_descriptor = opaque_material->pipeline();
    translucent_descriptor.blending.enabled = true;
    const auto translucent_material =
        std::make_shared<const mgv::Material>(std::move(translucent_descriptor));
    const auto instance = [](const std::shared_ptr<const mgv::Material>& material, float marker) {
        auto value = std::make_shared<mgv::MaterialInstance>(material);
        value->set_color("uMarker", {marker, 0.0F, 0.0F, 1.0F});
        return value;
    };
    mgv::Transform near;
    near.set_position({0.0F, 0.0F, 1.0F});
    mgv::Transform far;
    far.set_position({0.0F, 0.0F, -2.0F});
    mgv::Scene scene;
    scene.add(mgv::Renderable{mesh, instance(translucent_material, 4.0F), near});
    scene.add(mgv::Renderable{mesh, instance(opaque_material, 2.0F), far});
    scene.add(mgv::Renderable{mesh, instance(translucent_material, 3.0F), far});
    scene.add(mgv::Renderable{mesh, instance(opaque_material, 1.0F), near});

    renderer.set_scene(std::move(scene));
    renderer.render({.framebuffer_width = 800, .framebuffer_height = 600});

    CHECK(calls.draw_markers == std::vector<float>{1.0F, 2.0F, 3.0F, 4.0F});
    CHECK(renderer.last_frame_statistics() == mgv::RenderStatistics{
                                                   .submitted = 4,
                                                   .culled = 0,
                                                   .opaque = 2,
                                                   .translucent = 2,
                                               });
}

TEST_CASE("render queue preserves submission order for equal sort keys") {
    Calls calls;
    mgv::Renderer renderer{std::make_unique<FakeBackend>(calls)};
    const auto mesh = std::make_shared<const mgv::MeshData>(mgv::MeshData{
        .vertices = {{{-0.1F, -0.1F, 0.0F}, {}, {}},
                     {{0.1F, -0.1F, 0.0F}, {}, {}},
                     {{0.0F, 0.1F, 0.0F}, {}, {}}},
        .indices = {0, 1, 2},
    });
    const auto material = std::make_shared<const mgv::Material>(
        mgv::ShaderSources{"vertex", "fragment", "test.vert", "test.frag"});
    const auto instance = [&material](float marker) {
        auto value = std::make_shared<mgv::MaterialInstance>(material);
        value->set_color("uMarker", {marker, 0.0F, 0.0F, 1.0F});
        return value;
    };
    mgv::Scene scene;
    scene.add(mgv::Renderable{mesh, instance(8.0F)});
    scene.add(mgv::Renderable{mesh, instance(7.0F)});

    renderer.set_scene(std::move(scene));
    renderer.render({});

    CHECK(calls.draw_markers == std::vector<float>{8.0F, 7.0F});
}

TEST_CASE("renderer rejects an empty scene without touching the backend") {
    Calls calls;
    mgv::Renderer renderer{std::make_unique<FakeBackend>(calls)};

    CHECK_THROWS_AS(renderer.set_scene({}), std::invalid_argument);
    CHECK(calls.meshes == 0);
    CHECK(calls.pipelines == 0);
}

TEST_CASE("renderer creates explicit material pipeline state") {
    Calls calls;
    mgv::Renderer renderer{std::make_unique<FakeBackend>(calls)};
    const auto mesh = std::make_shared<const mgv::MeshData>(mgv::MeshData{
        .vertices = {{{0, 0, 0}, {0, 0, 1}, {0, 0}},
                     {{1, 0, 0}, {0, 0, 1}, {1, 0}},
                     {{0, 1, 0}, {0, 0, 1}, {0, 1}}},
        .indices = {0, 1, 2},
    });
    const auto material = std::make_shared<const mgv::Material>(mgv::RenderPipelineDescriptor{
        .shaders = {"vertex", "fragment", "test.vert", "test.frag"},
        .topology = mgv::PrimitiveTopology::line_list,
        .rasterization = {
            .cull_mode = mgv::CullMode::back,
            .front_face = mgv::FrontFace::counter_clockwise,
            .polygon_mode = mgv::PolygonMode::line,
        },
        .depth = {
            .test_enabled = true,
            .write_enabled = false,
            .compare = mgv::CompareOperation::less_or_equal,
        },
        .blending = {.enabled = true},
    });
    const auto material_instance = std::make_shared<const mgv::MaterialInstance>(material);
    mgv::Scene scene;
    scene.add(mgv::Renderable{mesh, material_instance});

    renderer.set_scene(std::move(scene));

    CHECK(calls.pipelines == 1);
    CHECK(calls.topology == mgv::PrimitiveTopology::line_list);
    CHECK(calls.cull_mode == mgv::CullMode::back);
    CHECK(calls.polygon_mode == mgv::PolygonMode::line);
    CHECK(calls.depth_compare == mgv::CompareOperation::less_or_equal);
    CHECK_FALSE(calls.depth_write);
    CHECK(calls.blending);
}

TEST_CASE("renderer binds and deduplicates shared textures") {
    Calls calls;
    mgv::Renderer renderer{std::make_unique<FakeBackend>(calls)};
    const auto mesh = std::make_shared<const mgv::MeshData>(mgv::MeshData{
        .vertices = {{{0, 0, 0}, {0, 0, 1}, {0, 0}},
                     {{1, 0, 0}, {0, 0, 1}, {1, 0}},
                     {{0, 1, 0}, {0, 0, 1}, {0, 1}}},
        .indices = {0, 1, 2},
    });
    const auto material = std::make_shared<const mgv::Material>(
        mgv::ShaderSources{"vertex", "fragment", "test.vert", "test.frag"});
    const auto texture = std::make_shared<const mgv::Texture>(mgv::ImageData{
        1,
        1,
        mgv::PixelFormat::rgba8_unorm,
        mgv::ColorSpace::srgb,
        {10, 20, 30, 255},
    });
    auto first = std::make_shared<mgv::MaterialInstance>(material);
    auto second = std::make_shared<mgv::MaterialInstance>(material);
    first->set_texture("uBaseColorTexture", texture);
    second->set_texture("uBaseColorTexture", texture);
    mgv::Scene scene;
    scene.add(mgv::Renderable{mesh, first});
    scene.add(mgv::Renderable{mesh, second});

    renderer.set_scene(std::move(scene));
    renderer.render({});

    CHECK(calls.pipelines == 1);
    CHECK(calls.textures == 1);
    CHECK(calls.samplers == 1);
    CHECK(calls.draws == 2);
    CHECK(calls.texture_binding_names ==
          std::vector<std::string>{"uBaseColorTexture", "uBaseColorTexture"});
}

TEST_CASE("failed sampler preparation leaves the previous scene renderable") {
    Calls calls;
    mgv::Renderer renderer{std::make_unique<FakeBackend>(calls)};
    mgv::MeshData mesh{
        .vertices = {{{0, 0, 0}, {0, 0, 1}, {0, 0}},
                     {{1, 0, 0}, {0, 0, 1}, {1, 0}},
                     {{0, 1, 0}, {0, 0, 1}, {0, 1}}},
        .indices = {0, 1, 2},
    };
    renderer.load(mesh, {"vertex", "fragment", "initial.vert", "initial.frag"});
    const auto material = std::make_shared<const mgv::Material>(
        mgv::ShaderSources{"vertex", "fragment", "test.vert", "test.frag"});
    auto instance = std::make_shared<mgv::MaterialInstance>(material);
    instance->set_texture("uBaseColorTexture", std::make_shared<const mgv::Texture>(mgv::ImageData{
                                                    1,
                                                    1,
                                                    mgv::PixelFormat::rgba8_unorm,
                                                    mgv::ColorSpace::linear,
                                                    {255, 255, 255, 255},
                                                }));
    mgv::Scene replacement;
    replacement.add(mgv::Renderable{std::make_shared<const mgv::MeshData>(mesh), instance});
    calls.reject_samplers = true;

    CHECK_THROWS(renderer.set_scene(std::move(replacement)));
    calls.reject_samplers = false;
    renderer.render({});

    CHECK(calls.draws == 1);
    CHECK(calls.texture_binding_names.empty());
}

TEST_CASE("renderer uses the backend clip-space convention") {
    Calls calls;
    mgv::RenderBackendCapabilities capabilities;
    capabilities.clip_space.depth_range = mgv::ClipDepthRange::zero_to_one;
    capabilities.clip_space.invert_y = true;
    mgv::Renderer renderer{std::make_unique<FakeBackend>(calls, capabilities)};
    mgv::MeshData mesh{
        .vertices = {{{0, 0, 0}, {0, 0, 1}, {0, 0}},
                     {{1, 0, 0}, {0, 0, 1}, {1, 0}},
                     {{0, 1, 0}, {0, 0, 1}, {0, 1}}},
        .indices = {0, 1, 2},
    };
    renderer.load(std::move(mesh), {"vertex", "fragment", "test.vert", "test.frag"});

    renderer.render({.framebuffer_width = 800, .framebuffer_height = 600});

    REQUIRE(calls.transforms.size() == 1);
    CHECK(calls.transforms.front()[5] < 0.0F);
}

TEST_CASE("renderer ends a begun frame when drawing throws") {
    Calls calls;
    mgv::Renderer renderer{std::make_unique<FakeBackend>(calls)};
    mgv::MeshData mesh{
        .vertices = {{{0, 0, 0}, {0, 0, 1}, {0, 0}},
                     {{1, 0, 0}, {0, 0, 1}, {1, 0}},
                     {{0, 1, 0}, {0, 0, 1}, {0, 1}}},
        .indices = {0, 1, 2},
    };
    renderer.load(std::move(mesh), {"vertex", "fragment", "test.vert", "test.frag"});
    calls.reject_draws = true;

    CHECK_THROWS(renderer.render({}));

    CHECK(calls.begins == 1);
    CHECK(calls.draws == 1);
    CHECK(calls.ends == 1);
}

TEST_CASE("failed scene preparation leaves the previous scene renderable") {
    Calls calls;
    mgv::Renderer renderer{std::make_unique<FakeBackend>(calls)};
    mgv::MeshData mesh{
        .vertices = {{{0, 0, 0}, {0, 0, 1}, {0, 0}},
                     {{1, 0, 0}, {0, 0, 1}, {1, 0}},
                     {{0, 1, 0}, {0, 0, 1}, {0, 1}}},
        .indices = {0, 1, 2},
    };
    renderer.load(mesh, {"vertex", "fragment", "initial.vert", "initial.frag"});
    mgv::Scene replacement;
    replacement.add(mgv::Renderable{
        std::make_shared<const mgv::MeshData>(mesh),
        std::make_shared<const mgv::MaterialInstance>(
            std::make_shared<const mgv::Material>(
                mgv::ShaderSources{"new vertex", "new fragment", "new.vert", "new.frag"}))});
    calls.reject_pipelines = true;

    CHECK_THROWS(renderer.set_scene(std::move(replacement)));
    calls.reject_pipelines = false;
    renderer.render({});

    CHECK(calls.draws == 1);
    CHECK(calls.begins == 1);
    CHECK(calls.ends == 1);
}

TEST_CASE("renderer publishes the camera matrices and position for the frame") {
    Calls calls;
    mgv::Renderer renderer{std::make_unique<FakeBackend>(calls)};
    renderer.load(unit_triangle(), {"vertex", "fragment", "test.vert", "test.frag"});
    mgv::Camera camera;
    camera.look_at({3.0F, 4.0F, 5.0F}, {0.0F, 1.0F, 0.0F});
    camera.set_perspective(50.0F, 0.25F, 500.0F);
    renderer.set_camera(camera);

    renderer.render({.framebuffer_width = 800, .framebuffer_height = 400});

    REQUIRE(calls.frame_camera_positions.size() == 1);
    CHECK(calls.frame_camera_positions.front() == mgv::Vec3{3.0F, 4.0F, 5.0F});

    const auto expected_view = camera.view_matrix();
    const auto expected_projection = camera.projection_matrix(2.0F);
    const auto expected_view_projection = camera.view_projection_matrix(2.0F);
    CHECK(calls.frame_views.front() == expected_view);
    CHECK(calls.frame_projections.front() == expected_projection);
    CHECK(calls.frame_view_projections.front() == expected_view_projection);
}

TEST_CASE("renderer forwards its environment to every frame") {
    Calls calls;
    mgv::Renderer renderer{std::make_unique<FakeBackend>(calls)};
    renderer.load(unit_triangle(), {"vertex", "fragment", "test.vert", "test.frag"});

    mgv::Environment environment;
    environment.sun.direction = {0.0F, 4.0F, 3.0F};
    environment.sun.illuminance = 1234.0F;
    environment.exposure = 0.25F;
    environment.fog_density = 0.02F;
    renderer.set_environment(environment);

    renderer.render({.framebuffer_width = 320, .framebuffer_height = 240});

    REQUIRE(calls.frame_environments.size() == 1);
    const auto& published = calls.frame_environments.front();
    CHECK(published.sun.illuminance == 1234.0F);
    CHECK(published.exposure == 0.25F);
    CHECK(published.fog_density == 0.02F);
    SECTION("the sun direction is normalized before it reaches the backend") {
        CHECK(published.sun.direction.y == Catch::Approx(0.8F));
        CHECK(published.sun.direction.z == Catch::Approx(0.6F));
    }
    CHECK(renderer.environment().sun.illuminance == 1234.0F);
}

TEST_CASE("renderer submits a world transform and a normal matrix per draw") {
    Calls calls;
    mgv::Renderer renderer{std::make_unique<FakeBackend>(calls)};
    const auto ids = renderer.load(unit_triangle(), {"vertex", "fragment", "test.vert", "test.frag"});
    REQUIRE(ids.size() == 1);

    mgv::Transform transform;
    transform.set_position({2.0F, 3.0F, 4.0F}).set_scale({2.0F, 4.0F, 8.0F});
    renderer.set_renderable_transform(ids.front(), transform);
    mgv::Camera camera;
    camera.look_at({2.0F, 3.0F, 40.0F}, {2.0F, 3.0F, 4.0F});
    camera.set_perspective(60.0F, 0.1F, 500.0F);
    renderer.set_camera(camera);

    renderer.render({.framebuffer_width = 100, .framebuffer_height = 100});

    REQUIRE(calls.models.size() == 1);
    const auto& model = calls.models.front();
    CHECK(model == transform.matrix());
    CHECK(model[12] == 2.0F);
    CHECK(model[13] == 3.0F);
    CHECK(model[14] == 4.0F);

    REQUIRE(calls.normal_matrices.size() == 1);
    const auto& normal = calls.normal_matrices.front();
    SECTION("non-uniform scale is inverted so normals stay perpendicular") {
        CHECK(normal[0] == Catch::Approx(0.5F));
        CHECK(normal[5] == Catch::Approx(0.25F));
        CHECK(normal[10] == Catch::Approx(0.125F));
    }
    SECTION("the normal matrix discards translation") {
        CHECK(normal[12] == Catch::Approx(0.0F));
        CHECK(normal[13] == Catch::Approx(0.0F));
        CHECK(normal[14] == Catch::Approx(0.0F));
    }
}

TEST_CASE("renderer forwards the wet trail it was given for the frame") {
    Calls calls;
    mgv::Renderer renderer{std::make_unique<FakeBackend>(calls)};
    renderer.load(unit_triangle(), {"vertex", "fragment", "test.vert", "test.frag"});

    const std::vector<mgv::Vec3> trail{{0.0F, 0.0F, 0.0F}, {1.0F, 0.0F, 1.0F}};
    renderer.render({
        .framebuffer_width = 100,
        .framebuffer_height = 100,
        .wet_trail = trail,
    });

    REQUIRE(calls.frame_trail_sizes.size() == 1);
    CHECK(calls.frame_trail_sizes.front() == 2);
}

TEST_CASE("an authored fit preserves the modelled units of an asset") {
    const std::filesystem::path fixtures{MGV_TEST_FIXTURES};
    const mgv::AssetPaths paths{
        .model = fixtures / "triangle.obj",
        .vertex_shader = fixtures / "basic.vert",
        .fragment_shader = fixtures / "basic.frag",
    };

    Calls fitted_calls;
    mgv::Renderer fitted{std::make_unique<FakeBackend>(fitted_calls)};
    static_cast<void>(fitted.load(paths, mgv::ModelFit::fit_to_view));
    const auto fitted_ids = fitted.last_frame_statistics();
    static_cast<void>(fitted_ids);

    Calls authored_calls;
    mgv::Renderer authored{std::make_unique<FakeBackend>(authored_calls)};
    const auto authored_ids = authored.load(paths, mgv::ModelFit::authored);
    REQUIRE(authored_ids.size() == 1);

    SECTION("an authored load applies no transform at all") {
        const auto transform = authored.renderable_transform(authored_ids.front());
        CHECK(transform.position() == mgv::Vec3{0.0F, 0.0F, 0.0F});
        CHECK(transform.scale() == mgv::Vec3{1.0F, 1.0F, 1.0F});
    }

    SECTION("the default load still rescales to fill the view") {
        const auto ids = fitted.load(paths);
        REQUIRE(ids.size() == 1);
        const auto transform = fitted.renderable_transform(ids.front());
        CHECK(transform.scale().x != 1.0F);
    }
}

TEST_CASE("renderer replaces geometry in place and rebounds it") {
    Calls calls;
    mgv::Renderer renderer{std::make_unique<FakeBackend>(calls)};
    const auto ids = renderer.load(unit_triangle(), {"vertex", "fragment", "test.vert", "test.frag"});
    REQUIRE(ids.size() == 1);
    const auto created = calls.meshes;

    mgv::MeshData larger{
        .vertices = {{{-20.0F, 0.0F, -20.0F}, {0, 1, 0}, {0, 0}},
                     {{20.0F, 0.0F, -20.0F}, {0, 1, 0}, {1, 0}},
                     {{0.0F, 0.0F, 20.0F}, {0, 1, 0}, {0, 1}},
                     {{0.0F, 3.0F, 0.0F}, {0, 1, 0}, {1, 1}}},
        .indices = {0, 1, 2, 0, 1, 3},
    };

    CHECK(renderer.update_mesh(ids.front(), larger));

    SECTION("the backend resource is reused rather than recreated") {
        CHECK(calls.meshes == created);
        CHECK(calls.mesh_updates == 1);
        CHECK(calls.last_updated_vertices == 4);
    }
    SECTION("the renderable keeps its identity and transform") {
        CHECK(renderer.renderable_transform(ids.front()).scale().x != 0.0F);
    }
    SECTION("the new geometry is visible where the old would have been culled") {
        // The replacement spans forty metres; a camera far off to the side
        // sees it only if the bounds were recalculated.
        mgv::Camera camera;
        camera.look_at({0.0F, 2.0F, 60.0F}, {0.0F, 0.0F, 0.0F});
        camera.set_perspective(45.0F, 0.1F, 500.0F);
        renderer.set_camera(camera);
        renderer.render({.framebuffer_width = 64, .framebuffer_height = 64});
        CHECK(renderer.last_frame_statistics().submitted == 1);
    }
}

TEST_CASE("renderer reports a backend that cannot replace geometry") {
    Calls calls;
    calls.refuse_mesh_updates = true;
    mgv::Renderer renderer{std::make_unique<FakeBackend>(calls)};
    const auto ids = renderer.load(unit_triangle(), {"vertex", "fragment", "test.vert", "test.frag"});

    CHECK_FALSE(renderer.update_mesh(ids.front(), unit_triangle()));
}

TEST_CASE("replacing the geometry of an unknown renderable is an error") {
    Calls calls;
    mgv::Renderer renderer{std::make_unique<FakeBackend>(calls)};
    static_cast<void>(renderer.load(unit_triangle(), {"vertex", "fragment", "test.vert", "test.frag"}));

    CHECK_THROWS_AS(renderer.update_mesh(mgv::RenderableId{9999}, unit_triangle()), std::out_of_range);
}
