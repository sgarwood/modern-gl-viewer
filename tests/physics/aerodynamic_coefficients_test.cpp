#include "mgv/physics/aerodynamics.hpp"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <cmath>

namespace {
constexpr float ball_radius = 0.021335F;
} // namespace

TEST_CASE("drag follows the published fit") {
    // C_D = 0.24 + 0.18 S + 0.06 sin(pi (Re - 90,000) / 200,000).
    // At the onset of the transition band the sine term is nothing of it.
    CHECK(mgv::physics::drag_coefficient(0.0F, 90'000.0F) == Catch::Approx(0.24F));
    // Halfway across the band it is all of it.
    CHECK(mgv::physics::drag_coefficient(0.0F, 190'000.0F) == Catch::Approx(0.30F));
    // Spin adds induced drag, linearly.
    CHECK(mgv::physics::drag_coefficient(0.2F, 90'000.0F) == Catch::Approx(0.276F));
    CHECK(mgv::physics::drag_coefficient(0.2F, 190'000.0F) == Catch::Approx(0.336F));
}

TEST_CASE("the drag transition is clamped to the band it was measured across") {
    // Switched off below the band rather than extrapolated...
    CHECK(mgv::physics::drag_coefficient(0.1F, 10'000.0F) ==
          Catch::Approx(mgv::physics::drag_coefficient(0.1F, 90'000.0F)));
    // ...and above it, so the term can never start subtracting drag, which is
    // what the bare sine would do beyond Re = 290,000.
    CHECK(mgv::physics::drag_coefficient(0.1F, 2'000'000.0F) == Catch::Approx(0.258F));
    for (const auto reynolds : {0.0F, 5.0e4F, 1.5e5F, 2.9e5F, 1.0e7F}) {
        CHECK(mgv::physics::drag_coefficient(0.0F, reynolds) >= 0.24F);
    }
}

TEST_CASE("lift rises with spin, with diminishing returns") {
    // C_L = 0.54 S^0.4. No spin, no lift.
    CHECK(mgv::physics::lift_coefficient(0.0F) == Catch::Approx(0.0F));
    CHECK(mgv::physics::lift_coefficient(0.2F) == Catch::Approx(0.2837F).margin(0.0005F));
    // An exponent well under one: doubling the spin ratio buys about a third
    // more lift, not twice as much. The constant 0.2 this replaced was wrong
    // in both directions at once -- too much lift for a driver turning at
    // 2700, far too little for a wedge turning at 9000.
    CHECK(mgv::physics::lift_coefficient(0.4F) / mgv::physics::lift_coefficient(0.2F) ==
          Catch::Approx(std::pow(2.0F, 0.4F)).margin(0.001F));
}

TEST_CASE("spin decay reproduces the published spin-down rate") {
    // Smits and Smith measured dw/dt = -4.0e-6 v^2 S / R^2 with v in mph,
    // which with S = R w / v is -4.0e-6 v w / R. Checked against it in those
    // units, since that is the form the number was published in and the
    // conversion is the only place this can go wrong.
    const auto speed_mps = 60.0F;
    const auto speed_mph = speed_mps * 2.236936F;
    const auto spin = 300.0F;
    const auto published = 4.0e-6F * speed_mph * spin / ball_radius;

    CHECK(mgv::physics::spin_decay_rate(speed_mps, ball_radius, 1.225F) * spin ==
          Catch::Approx(published).epsilon(0.001F));
}

TEST_CASE("spin holds for longer in thinner air") {
    // The published rate carries no density term. It is scaled by one here
    // because the moment doing the slowing is aerodynamic, so a course a mile
    // up should hold spin longer rather than identically.
    CHECK(mgv::physics::spin_decay_rate(60.0F, ball_radius, 1.0F) <
          mgv::physics::spin_decay_rate(60.0F, ball_radius, 1.225F));
    // A ball going nowhere keeps its spin, however long it is left.
    CHECK(mgv::physics::spin_decay_rate(0.0F, ball_radius, 1.225F) == 0.0F);
}

TEST_CASE("reynolds number follows the air the ball is flying through") {
    // v D / nu, with the kinematic viscosity taken from the density.
    CHECK(mgv::physics::reynolds_number(74.65F, ball_radius, 1.225F) ==
          Catch::Approx(215'600.0F).margin(300.0F));
    CHECK(mgv::physics::reynolds_number(74.65F, ball_radius, 1.0F) <
          mgv::physics::reynolds_number(74.65F, ball_radius, 1.225F));
    CHECK(mgv::physics::reynolds_number(0.0F, ball_radius, 1.225F) == 0.0F);
}

TEST_CASE("spin ratio is surface speed over flight speed") {
    CHECK(mgv::physics::spin_ratio(60.0F, 300.0F, ball_radius) ==
          Catch::Approx(0.10667F).margin(0.0001F));
    // Finite at rest, which is the only answer that keeps the coefficients
    // finite with it.
    CHECK(mgv::physics::spin_ratio(0.0F, 300.0F, ball_radius) == 0.0F);
}
