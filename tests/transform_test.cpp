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

TEST_CASE("quaternion composition applies the right-hand rotation first") {
    constexpr float quarter_turn = 1.57079633F;
    const auto yaw = mgv::Quaternion::from_axis_angle({0.0F, 1.0F, 0.0F}, quarter_turn);
    const auto pitch = mgv::Quaternion::from_axis_angle({1.0F, 0.0F, 0.0F}, quarter_turn);

    mgv::Transform composed;
    composed.set_rotation(yaw * pitch);
    mgv::Transform stepwise_yaw;
    stepwise_yaw.set_rotation(yaw);

    // Pitch takes +Y to +Z; yaw then takes +Z to +X. So up ends up pointing
    // along +X, and the matrix's Y column -- where +Y lands -- is (1, 0, 0).
    const auto matrix = composed.matrix();
    CHECK(matrix[4] == Catch::Approx(1.0F).margin(1.0e-5F));
    CHECK(matrix[5] == Catch::Approx(0.0F).margin(1.0e-5F));
    CHECK(matrix[6] == Catch::Approx(0.0F).margin(1.0e-5F));

    SECTION("composition is not commutative, so the order is load-bearing") {
        mgv::Transform other;
        other.set_rotation(pitch * yaw);
        CHECK(other.matrix() != matrix);
    }
    SECTION("composing with the identity changes nothing") {
        mgv::Transform same;
        same.set_rotation(yaw * mgv::Quaternion{});
        CHECK(same.matrix() == stepwise_yaw.matrix());
    }
}

TEST_CASE("a transform can be recovered from its own matrix") {
    mgv::Transform original;
    original.set_position({3.0F, -2.0F, 7.5F})
        .set_rotation(mgv::Quaternion::from_axis_angle({0.0F, 1.0F, 0.0F}, 1.1F) *
                      mgv::Quaternion::from_axis_angle({1.0F, 0.0F, 0.0F}, -0.4F))
        .set_uniform_scale(0.25F);

    const auto recovered = mgv::Transform::from_matrix(original.matrix());

    SECTION("the position comes back exactly") {
        CHECK(recovered.position().x == Catch::Approx(3.0F));
        CHECK(recovered.position().y == Catch::Approx(-2.0F));
        CHECK(recovered.position().z == Catch::Approx(7.5F));
    }
    SECTION("the scale comes back") {
        CHECK(recovered.scale().x == Catch::Approx(0.25F).margin(1.0e-5F));
    }
    SECTION("and the rotation round-trips through its matrix") {
        // Quaternions double-cover rotations, so q and -q are the same
        // turn; comparing the matrices avoids calling that a failure.
        const auto before = original.matrix();
        const auto after = recovered.matrix();
        for (std::size_t element = 0; element < before.size(); ++element) {
            CHECK(after[element] == Catch::Approx(before[element]).margin(1.0e-5F));
        }
    }
}

TEST_CASE("decomposition survives rotations near a half turn") {
    // The naive w-first reconstruction divides by something that goes to
    // zero at 180 degrees, which is exactly where a club pointing backwards
    // would put it.
    for (const float radians : {3.14159F, 3.0F, -3.1F, 1.5707963F}) {
        mgv::Transform original;
        original.set_rotation(
            mgv::Quaternion::from_axis_angle({0.0F, 0.0F, 1.0F}, radians));
        const auto recovered = mgv::Transform::from_matrix(original.matrix());
        const auto before = original.matrix();
        const auto after = recovered.matrix();
        for (std::size_t element = 0; element < before.size(); ++element) {
            CHECK(after[element] == Catch::Approx(before[element]).margin(1.0e-4F));
        }
    }
}
