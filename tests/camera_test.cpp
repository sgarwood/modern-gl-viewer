#include "mgv/camera.hpp"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <stdexcept>

TEST_CASE("default camera uses a right-handed Y-up view looking down negative Z") {
    const mgv::Camera camera;
    const auto view = camera.view_matrix();

    CHECK(camera.position() == mgv::Vec3{0.0F, 0.0F, 3.0F});
    CHECK(camera.target() == mgv::Vec3{0.0F, 0.0F, 0.0F});
    CHECK(view[0] == Catch::Approx(1.0F));
    CHECK(view[5] == Catch::Approx(1.0F));
    CHECK(view[10] == Catch::Approx(1.0F));
    CHECK(view[14] == Catch::Approx(-3.0F));
}

TEST_CASE("camera creates an OpenGL perspective projection") {
    mgv::Camera camera;
    camera.set_perspective(60.0F, 0.25F, 250.0F);

    const auto projection = camera.projection_matrix(16.0F / 9.0F);

    CHECK(projection[0] == Catch::Approx(projection[5] / (16.0F / 9.0F)));
    CHECK(projection[11] == Catch::Approx(-1.0F));
    CHECK(projection[15] == Catch::Approx(0.0F));
}

TEST_CASE("camera projection follows the backend clip-space convention") {
    mgv::Camera camera;
    camera.set_perspective(60.0F, 0.25F, 250.0F);

    const auto opengl = camera.projection_matrix(1.0F);
    const auto zero_to_one = camera.projection_matrix(1.0F, {
        .depth_range = mgv::ClipDepthRange::zero_to_one,
        .invert_y = true,
    });

    CHECK(zero_to_one[5] == Catch::Approx(-opengl[5]));
    CHECK(zero_to_one[10] != Catch::Approx(opengl[10]));
    CHECK(zero_to_one[14] != Catch::Approx(opengl[14]));
}

TEST_CASE("camera rejects degenerate poses and invalid projection parameters") {
    mgv::Camera camera;

    CHECK_THROWS_AS(camera.look_at({1, 1, 1}, {1, 1, 1}), std::invalid_argument);
    CHECK_THROWS_AS(camera.look_at({0, 0, 1}, {0, 0, 0}, {0, 0, 1}), std::invalid_argument);
    CHECK_THROWS_AS(camera.set_perspective(0.0F, 0.1F, 10.0F), std::invalid_argument);
    CHECK_THROWS_AS(camera.set_perspective(45.0F, 10.0F, 1.0F), std::invalid_argument);
    CHECK_THROWS_AS(camera.projection_matrix(0.0F), std::invalid_argument);
}
