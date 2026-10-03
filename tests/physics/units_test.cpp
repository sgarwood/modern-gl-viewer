#include "mgv/physics/units.hpp"

#include <catch2/catch_test_macros.hpp>

#include <stdexcept>

TEST_CASE("physics scalar units preserve their dimensions") {
    const mgv::physics::Duration duration{0.25F};
    const mgv::physics::Length length{2.5F};
    const mgv::physics::Mass mass{4.0F};

    CHECK(duration.seconds() == 0.25F);
    CHECK(length.metres() == 2.5F);
    CHECK(mass.kilograms() == 4.0F);
}

TEST_CASE("physics units reject invalid domain values") {
    CHECK_THROWS_AS(mgv::physics::Duration{-0.1F}, std::invalid_argument);
    CHECK_THROWS_AS(mgv::physics::Length{-0.1F}, std::invalid_argument);
    CHECK_THROWS_AS(mgv::physics::Mass{0.0F}, std::invalid_argument);
}

TEST_CASE("physics vector units remain explicit") {
    const mgv::physics::Position position{{1.0F, 2.0F, 3.0F}};
    const mgv::physics::LinearVelocity velocity{{4.0F, 5.0F, 6.0F}};
    const mgv::physics::Acceleration acceleration{{0.0F, -9.81F, 0.0F}};
    const mgv::physics::Impulse impulse{{2.0F, 0.0F, 0.0F}};

    CHECK(position.metres() == mgv::Vec3{1.0F, 2.0F, 3.0F});
    CHECK(velocity.metres_per_second() == mgv::Vec3{4.0F, 5.0F, 6.0F});
    CHECK(acceleration.metres_per_second_squared() == mgv::Vec3{0.0F, -9.81F, 0.0F});
    CHECK(impulse.newton_seconds() == mgv::Vec3{2.0F, 0.0F, 0.0F});
}
