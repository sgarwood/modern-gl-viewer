#include "mgv/bounds.hpp"
#include "mgv/camera.hpp"
#include "mgv/transform.hpp"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <cmath>
#include <stdexcept>

namespace {

[[nodiscard]] mgv::MeshData mesh() {
    return {
        .vertices = {{{-1.0F, -2.0F, -3.0F}, {}, {}},
                     {{3.0F, 2.0F, 1.0F}, {}, {}},
                     {{-1.0F, 2.0F, 1.0F}, {}, {}}},
        .indices = {0, 1, 2},
    };
}

} // namespace

TEST_CASE("mesh bounds enclose every vertex") {
    const auto bounds = mgv::calculate_bounds(mesh());

    CHECK(bounds.box.minimum == mgv::Vec3{-1.0F, -2.0F, -3.0F});
    CHECK(bounds.box.maximum == mgv::Vec3{3.0F, 2.0F, 1.0F});
    CHECK(bounds.sphere.center == mgv::Vec3{1.0F, 0.0F, -1.0F});
    CHECK(bounds.sphere.radius == Catch::Approx(3.4641016F));
}

TEST_CASE("bounds reject empty geometry") {
    CHECK_THROWS_AS(mgv::calculate_bounds({}), std::invalid_argument);
}

TEST_CASE("bounding spheres follow model translation rotation and scale") {
    mgv::Transform transform;
    transform.set_position({3.0F, 4.0F, 5.0F})
        .set_rotation(mgv::Quaternion::from_axis_angle({0.0F, 1.0F, 0.0F}, 1.0F))
        .set_scale({2.0F, 3.0F, 4.0F});

    const auto result = mgv::transform_bounds({.center = {}, .radius = 2.0F}, transform.matrix());

    CHECK(result.center.x == Catch::Approx(3.0F));
    CHECK(result.center.y == Catch::Approx(4.0F));
    CHECK(result.center.z == Catch::Approx(5.0F));
    CHECK(result.radius == Catch::Approx(8.0F));
}

TEST_CASE("frustum tests spheres for both portable clip-depth conventions") {
    mgv::Camera camera;
    const auto opengl = mgv::Frustum::from_view_projection(
        camera.view_projection_matrix(1.0F));
    mgv::ClipSpaceConvention portable_convention;
    portable_convention.depth_range = mgv::ClipDepthRange::zero_to_one;
    const auto portable = mgv::Frustum::from_view_projection(
        camera.view_projection_matrix(1.0F, portable_convention),
        portable_convention);

    for (const auto* frustum : {&opengl, &portable}) {
        CHECK(frustum->intersects({.center = {}, .radius = 0.5F}));
        CHECK_FALSE(frustum->intersects({.center = {100.0F, 0.0F, 0.0F}, .radius = 0.5F}));
        CHECK_FALSE(frustum->intersects({.center = {0.0F, 0.0F, 4.0F}, .radius = 0.25F}));
    }
}

TEST_CASE("bounds of an instanced mesh enclose every placement") {
    mgv::MeshData mesh{
        .vertices = {{{-0.5F, 0.0F, 0.0F}, {0, 0, 1}, {0, 0}},
                     {{0.5F, 0.0F, 0.0F}, {0, 0, 1}, {1, 0}},
                     {{0.0F, 1.0F, 0.0F}, {0, 0, 1}, {0, 1}}},
        .indices = {0, 1, 2},
    };
    mesh.instances = {
        {.position = {10.0F, 0.0F, 0.0F}},
        {.position = {-10.0F, 0.0F, 0.0F}},
        {.position = {0.0F, 0.0F, 25.0F}},
    };

    const auto bounds = mgv::calculate_bounds(mesh);

    SECTION("the box spans the placements, not just the source geometry") {
        CHECK(bounds.box.minimum.x <= -10.5F);
        CHECK(bounds.box.maximum.x >= 10.5F);
        CHECK(bounds.box.maximum.z >= 25.0F);
    }
    SECTION("the sphere contains every placed vertex") {
        for (const auto& instance : mesh.instances) {
            for (const auto& vertex : mesh.vertices) {
                const mgv::Vec3 placed{
                    vertex.position.x + instance.position.x,
                    vertex.position.y + instance.position.y,
                    vertex.position.z + instance.position.z,
                };
                const auto dx = placed.x - bounds.sphere.center.x;
                const auto dy = placed.y - bounds.sphere.center.y;
                const auto dz = placed.z - bounds.sphere.center.z;
                CHECK(std::sqrt(dx * dx + dy * dy + dz * dz) <= bounds.sphere.radius + 1.0e-4F);
            }
        }
    }
}

TEST_CASE("instance scale and yaw widen the bounds") {
    mgv::MeshData mesh{
        .vertices = {{{0.0F, 0.0F, 0.0F}, {0, 1, 0}, {0, 0}},
                     {{2.0F, 0.0F, 0.0F}, {0, 1, 0}, {1, 0}},
                     {{0.0F, 0.0F, 0.1F}, {0, 1, 0}, {0, 1}}},
        .indices = {0, 1, 2},
    };
    const auto unplaced = mgv::calculate_bounds(mesh);

    // A quarter turn swings the long axis onto Z, and the scale triples it.
    mesh.instances = {{.position = {}, .yaw = 1.5707963F, .parameters = {3.0F, 0, 0, 0}}};
    const auto placed = mgv::calculate_bounds(mesh);

    CHECK(placed.sphere.radius > unplaced.sphere.radius * 2.5F);
    CHECK(std::abs(placed.box.minimum.z) + std::abs(placed.box.maximum.z) > 5.0F);
}

TEST_CASE("an uninstanced mesh keeps the bounds it always had") {
    const mgv::MeshData mesh{
        .vertices = {{{-1.0F, 0.0F, 0.0F}, {0, 0, 1}, {0, 0}},
                     {{1.0F, 0.0F, 0.0F}, {0, 0, 1}, {1, 0}},
                     {{0.0F, 2.0F, 0.0F}, {0, 0, 1}, {0, 1}}},
        .indices = {0, 1, 2},
    };

    const auto bounds = mgv::calculate_bounds(mesh);

    CHECK(bounds.box.minimum.x == -1.0F);
    CHECK(bounds.box.maximum.y == 2.0F);
}
