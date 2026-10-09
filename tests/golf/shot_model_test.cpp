#include "mgv/golf/shot_model.hpp"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <limits>
#include <numbers>
#include <stdexcept>

TEST_CASE("standard shot model maps a full swing into Y-up ball kinematics") {
    const mgv::golf::StandardShotModel model;

    const auto launch = model.resolve(mgv::golf::FullSwingData{
        .ball_speed_mps = 50.0F,
        .launch_angle_deg = 30.0F,
        .launch_direction_deg = 0.0F,
        .total_spin_rpm = 3'000.0F,
        .spin_axis_deg = 0.0F,
    });

    const auto velocity = launch.linear_velocity.metres_per_second();
    CHECK(velocity.x == Catch::Approx(0.0F).margin(0.0001F));
    CHECK(velocity.y == Catch::Approx(25.0F).margin(0.0001F));
    CHECK(velocity.z == Catch::Approx(-43.30127F).margin(0.0001F));

    const auto spin = launch.angular_velocity.radians_per_second();
    CHECK(spin.x == Catch::Approx(100.0F * std::numbers::pi_v<float>).margin(0.001F));
    CHECK(spin.y == Catch::Approx(0.0F).margin(0.0001F));
    CHECK(spin.z == Catch::Approx(0.0F).margin(0.0001F));
}

TEST_CASE("standard shot model applies direction and spin axis in the shot frame") {
    const mgv::golf::StandardShotModel model;

    const auto launch = model.resolve(mgv::golf::FullSwingData{
        .ball_speed_mps = 10.0F,
        .launch_direction_deg = 90.0F,
        .total_spin_rpm = 60.0F,
        .spin_axis_deg = 90.0F,
    });

    const auto velocity = launch.linear_velocity.metres_per_second();
    CHECK(velocity.x == Catch::Approx(10.0F));
    CHECK(velocity.z == Catch::Approx(0.0F).margin(0.0001F));

    const auto spin = launch.angular_velocity.radians_per_second();
    CHECK(spin.x == Catch::Approx(0.0F).margin(0.0001F));
    CHECK(spin.y == Catch::Approx(2.0F * std::numbers::pi_v<float>).margin(0.0001F));
    CHECK(spin.z == Catch::Approx(0.0F).margin(0.0001F));
}

TEST_CASE("standard shot model turns a putter stroke into a rolling launch") {
    const mgv::golf::StandardShotModel model{{
        .putting_smash_factor = 1.6F,
        .ball_radius = mgv::physics::Length{0.02F},
    }};

    const auto launch = model.resolve(mgv::golf::PuttingData{
        .putter_speed_mps = 2.0F,
        .face_angle_deg = 0.0F,
    });

    const auto velocity = launch.linear_velocity.metres_per_second();
    CHECK(velocity.x == Catch::Approx(0.0F).margin(0.0001F));
    CHECK(velocity.y == Catch::Approx(0.0F));
    CHECK(velocity.z == Catch::Approx(-3.2F));

    // Rolling, not checking: a ball travelling along -Z rolls forward about
    // -X. Reversed, this is the backspin the full swing spins about, and the
    // putt would skid instead of running.
    const auto spin = launch.angular_velocity.radians_per_second();
    CHECK(spin.x == Catch::Approx(-160.0F));
    CHECK(spin.y == Catch::Approx(0.0F));
    CHECK(spin.z == Catch::Approx(0.0F));
    // The no-slip condition, stated as the solver reads it: the surface of
    // the ball is stationary where it touches the ground.
    CHECK(velocity.z - spin.x * 0.02F == Catch::Approx(0.0F).margin(0.0001F));
}

TEST_CASE("standard shot model rejects impossible or non-finite measurements") {
    CHECK_THROWS_AS(
        mgv::golf::StandardShotModel{{.putting_smash_factor = 0.0F}},
        std::invalid_argument);

    const mgv::golf::StandardShotModel model;
    CHECK_THROWS_AS(
        model.resolve(mgv::golf::FullSwingData{.ball_speed_mps = -1.0F}),
        std::invalid_argument);
    CHECK_THROWS_AS(
        model.resolve(mgv::golf::FullSwingData{
            .ball_speed_mps = 1.0F,
            .launch_angle_deg = 91.0F,
        }),
        std::invalid_argument);
    CHECK_THROWS_AS(
        model.resolve(mgv::golf::PuttingData{
            .putter_speed_mps = std::numeric_limits<float>::quiet_NaN(),
        }),
        std::invalid_argument);
}
