#include "mgv/physics/units.hpp"
#include <catch2/catch_approx.hpp>

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

TEST_CASE("air density at sea level matches the textbook figure") {
    // Dry air at fifteen degrees and standard pressure is 1.225 kg/m^3, the
    // value the ISA is defined by and the one the aerodynamics default to.
    CHECK(mgv::physics::air_density(15.0F) == Catch::Approx(1.225F).epsilon(0.002F));
}

TEST_CASE("cold air is denser than warm air") {
    CHECK(mgv::physics::air_density(0.0F) > mgv::physics::air_density(30.0F));
}

TEST_CASE("altitude thins the air, which is why a ball carries further up a mountain") {
    // Denver sits around 1600 m, where the pressure is near 83 kPa.
    const auto sea_level = mgv::physics::air_density(20.0F);
    const auto mile_high = mgv::physics::air_density(20.0F, 83'000.0F);

    CHECK(mile_high < sea_level);
    // Roughly a sixth less dense, which is most of the reason balls fly in
    // Denver. Taking the pressure as sea level everywhere discards it.
    CHECK(sea_level / mile_high == Catch::Approx(1.22F).epsilon(0.03F));
}

TEST_CASE("humid air is less dense than dry air, not more") {
    // A water molecule is lighter than the nitrogen and oxygen it displaces,
    // so saturated air is thinner. This is the opposite of what most people
    // expect, and it is why it is tested.
    const auto dry = mgv::physics::air_density(30.0F, 101'325.0F, 0.0F);
    const auto saturated = mgv::physics::air_density(30.0F, 101'325.0F, 1.0F);

    CHECK(saturated < dry);
    // The effect is real but small: around two percent on a hot, wet day.
    CHECK(dry - saturated == Catch::Approx(0.019F).margin(0.006F));
}

TEST_CASE("air density copes with nonsense inputs rather than returning one") {
    CHECK(mgv::physics::air_density(-300.0F) > 0.0F);
    CHECK(mgv::physics::air_density(20.0F, -5.0F) >= 0.0F);
    CHECK(mgv::physics::air_density(20.0F, 101'325.0F, 7.0F) > 0.0F);
    CHECK(mgv::physics::air_density(20.0F, 101'325.0F, -7.0F) > 0.0F);
}
