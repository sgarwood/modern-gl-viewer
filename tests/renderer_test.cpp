#include "mgv/renderer.hpp"

#include <catch2/catch_test_macros.hpp>

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
    mgv::PrimitiveTopology topology{mgv::PrimitiveTopology::triangle_list};
    mgv::CullMode cull_mode{mgv::CullMode::none};
    mgv::PolygonMode polygon_mode{mgv::PolygonMode::fill};
    mgv::CompareOperation depth_compare{mgv::CompareOperation::less};
    bool depth_write{};
    bool blending{};
    std::vector<std::string> texture_binding_names;
    std::vector<mgv::Mat4> transforms;
};

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
    void begin_frame(const mgv::Frame&) override { ++calls_.begins; }
    void draw(const mgv::DrawPacket& packet) override {
        ++calls_.draws;
        calls_.transforms.push_back(packet.model_view_projection);
        for (const auto& binding : packet.textures) {
            REQUIRE(binding.texture != nullptr);
            REQUIRE(binding.sampler != nullptr);
            calls_.texture_binding_names.push_back(binding.name);
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
    moved.set_position({2.0F, 0.0F, 0.0F});
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
