#include "mgv/bounds.hpp"
#include "mgv/camera.hpp"
#include "mgv/transform.hpp"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

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
