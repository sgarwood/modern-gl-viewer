#include "mgv/renderer.hpp"

#include <catch2/catch_test_macros.hpp>

#include <memory>
#include <stdexcept>
#include <vector>

namespace {

struct Calls final {
    int meshes{};
    int shaders{};
    int begins{};
    int draws{};
    int ends{};
    bool reject_shaders{};
    std::vector<mgv::Mat4> transforms;
};

class FakeMesh final : public mgv::MeshResource {};
class FakeShader final : public mgv::ShaderResource {};

class FakeBackend final : public mgv::RenderBackend {
public:
    explicit FakeBackend(Calls& calls) : calls_{calls} {}

    std::unique_ptr<mgv::MeshResource> create_mesh(const mgv::MeshData&) override {
        ++calls_.meshes;
        return std::make_unique<FakeMesh>();
    }
    std::unique_ptr<mgv::ShaderResource> create_shader(const mgv::ShaderSources&) override {
        ++calls_.shaders;
        if (calls_.reject_shaders) {
            return nullptr;
        }
        return std::make_unique<FakeShader>();
    }
    void begin_frame(const mgv::Frame&) override { ++calls_.begins; }
    void draw(const mgv::MeshResource&, const mgv::ShaderResource&, const mgv::Mat4& transform) override {
        ++calls_.draws;
        calls_.transforms.push_back(transform);
    }
    void end_frame() override { ++calls_.ends; }

private:
    Calls& calls_;
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
    CHECK(calls.shaders == 1);
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
    CHECK(calls.shaders == 0);
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
    mgv::Transform moved;
    moved.set_position({2.0F, 0.0F, 0.0F});
    mgv::Scene scene;
    scene.add(mgv::Renderable{mesh, material});
    scene.add(mgv::Renderable{mesh, material, moved});
    scene.add(mgv::Renderable{mesh, material}.set_visible(false));

    renderer.set_scene(std::move(scene));
    renderer.render({.framebuffer_width = 800, .framebuffer_height = 600});

    CHECK(calls.meshes == 1);
    CHECK(calls.shaders == 1);
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
    CHECK(calls.shaders == 0);
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
        std::make_shared<const mgv::Material>(
            mgv::ShaderSources{"new vertex", "new fragment", "new.vert", "new.frag"})});
    calls.reject_shaders = true;

    CHECK_THROWS(renderer.set_scene(std::move(replacement)));
    calls.reject_shaders = false;
    renderer.render({});

    CHECK(calls.draws == 1);
    CHECK(calls.begins == 1);
    CHECK(calls.ends == 1);
}
