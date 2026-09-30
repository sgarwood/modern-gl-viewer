#include "mgv/transform.hpp"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <numbers>
#include <stdexcept>

TEST_CASE("default transform is identity") {
    const auto matrix = mgv::Transform{}.matrix();
    const mgv::Mat4 identity{
        1, 0, 0, 0,
        0, 1, 0, 0,
        0, 0, 1, 0,
        0, 0, 0, 1,
    };

    CHECK(matrix == identity);
}

TEST_CASE("transform composes translation rotation and non-uniform scale") {
    mgv::Transform transform;
    transform.set_position({2.0F, 3.0F, 4.0F})
        .set_rotation(mgv::Quaternion::from_axis_angle({0.0F, 1.0F, 0.0F}, std::numbers::pi_v<float> / 2.0F))
        .set_scale({2.0F, 3.0F, 4.0F});

    const auto matrix = transform.matrix();

    CHECK(matrix[0] == Catch::Approx(0.0F).margin(1.0e-5F));
    CHECK(matrix[2] == Catch::Approx(-2.0F));
    CHECK(matrix[5] == Catch::Approx(3.0F));
    CHECK(matrix[8] == Catch::Approx(4.0F));
    CHECK(matrix[10] == Catch::Approx(0.0F).margin(1.0e-5F));
    CHECK(matrix[12] == Catch::Approx(2.0F));
    CHECK(matrix[13] == Catch::Approx(3.0F));
    CHECK(matrix[14] == Catch::Approx(4.0F));
}

TEST_CASE("quaternion construction rejects a zero axis") {
    CHECK_THROWS_AS(mgv::Quaternion::from_axis_angle({}, 1.0F), std::invalid_argument);
}
