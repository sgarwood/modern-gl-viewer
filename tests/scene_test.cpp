#include "mgv/scene.hpp"

#include <catch2/catch_test_macros.hpp>

#include <memory>
#include <stdexcept>

namespace {

[[nodiscard]] std::shared_ptr<const mgv::MeshData> triangle_mesh() {
    return std::make_shared<const mgv::MeshData>(mgv::MeshData{
        .vertices = {{{0, 0, 0}, {0, 0, 1}, {0, 0}},
                     {{1, 0, 0}, {0, 0, 1}, {1, 0}},
                     {{0, 1, 0}, {0, 0, 1}, {0, 1}}},
        .indices = {0, 1, 2},
    });
}

[[nodiscard]] std::shared_ptr<const mgv::MaterialInstance> material() {
    return std::make_shared<const mgv::MaterialInstance>(
        std::make_shared<const mgv::Material>(
            mgv::ShaderSources{"vertex", "fragment", "test.vert", "test.frag"}));
}

} // namespace

TEST_CASE("scene stores validated renderables and their transforms") {
    mgv::Transform transform;
    transform.set_position({1.0F, 2.0F, 3.0F});
    mgv::Scene scene;

    const auto id = scene.add(mgv::Renderable{triangle_mesh(), material(), transform});

    REQUIRE(scene.size() == 1);
    CHECK(!scene.empty());
    CHECK(scene.renderable(id).transform().position() == mgv::Vec3{1.0F, 2.0F, 3.0F});
    CHECK(scene.renderables().front().visible());
}

TEST_CASE("scene handles remain stable when other renderables are removed") {
    mgv::Scene scene;
    const auto removed = scene.add(mgv::Renderable{triangle_mesh(), material()});
    const auto retained = scene.add(mgv::Renderable{triangle_mesh(), material()});

    CHECK(scene.remove(removed));
    CHECK_FALSE(scene.contains(removed));
    CHECK(scene.contains(retained));
    CHECK_THROWS_AS(scene.renderable(removed), std::out_of_range);

    mgv::Transform moved;
    moved.set_position({4.0F, 5.0F, 6.0F});
    scene.renderable(retained).set_transform(moved);
    const auto replacement = scene.add(mgv::Renderable{triangle_mesh(), material()});

    CHECK(replacement != removed);
    CHECK(scene.renderable(retained).transform().position() == mgv::Vec3{4.0F, 5.0F, 6.0F});
}

TEST_CASE("renderable rejects missing or empty resources") {
    const auto empty_mesh = std::make_shared<const mgv::MeshData>();

    CHECK_THROWS_AS((mgv::Renderable{nullptr, material()}), std::invalid_argument);
    CHECK_THROWS_AS((mgv::Renderable{empty_mesh, material()}), std::invalid_argument);
    CHECK_THROWS_AS((mgv::Renderable{triangle_mesh(), nullptr}), std::invalid_argument);
    CHECK_THROWS_AS((mgv::Material{mgv::ShaderSources{}}), std::invalid_argument);
}

TEST_CASE("renderable visibility and transform are mutable before scene submission") {
    mgv::Renderable renderable{triangle_mesh(), material()};
    mgv::Transform transform;
    transform.set_uniform_scale(2.0F);

    renderable.set_visible(false).set_transform(transform);

    CHECK_FALSE(renderable.visible());
    CHECK(renderable.transform().scale() == mgv::Vec3{2.0F, 2.0F, 2.0F});
}
