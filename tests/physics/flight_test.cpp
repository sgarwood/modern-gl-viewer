#include "mgv/physics/physics_world.hpp"
#include "mgv/physics/rigid_body.hpp"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <cmath>

namespace {

constexpr float ball_radius = 0.021335F;
constexpr float ball_mass = 0.04593F;
constexpr float step = 1.0F / 480.0F;
constexpr float rpm_to_radians_per_second = 0.104719755F;
constexpr float degrees_to_radians = 3.14159265F / 180.0F;

struct Flight final {
    float carry{};
    float apex{};
    float descent_degrees{};
    float seconds{};
    /// Spin at the moment it lands, as a fraction of what it launched with.
    float retained_spin{};
};

/// Launches a ball and watches it until it comes back down to the height it
/// left from.
[[nodiscard]] Flight struck(
    float ball_speed,
    float launch_degrees,
    float spin_rpm,
    float air_density = 1.225F) {
    const auto launch = launch_degrees * degrees_to_radians;
    const auto spin = spin_rpm * rpm_to_radians_per_second;

    mgv::physics::PhysicsConfiguration configuration;
    configuration.fixed_time_step = mgv::physics::Duration{step};
    configuration.gravity = mgv::physics::Acceleration{{0.0F, -9.81F, 0.0F}};
    configuration.air_density = air_density;
    mgv::physics::PhysicsWorld world{configuration};

    const auto ball = world.add_body(
        mgv::physics::RigidBodyBuilder{
            mgv::physics::Collider::sphere(mgv::physics::Length{ball_radius})}
            .velocity(mgv::physics::LinearVelocity{{
                ball_speed * std::cos(launch),
                ball_speed * std::sin(launch),
                0.0F,
            }})
            // Backspin, for a ball travelling along +X.
            .angular_velocity(mgv::physics::AngularVelocity{{0.0F, 0.0F, spin}})
            .mass(mgv::physics::Mass{ball_mass})
            .build());

    Flight flight;
    auto previous = world.body(ball).position().metres();
    for (int tick = 0; tick < 10'000; ++tick) {
        previous = world.body(ball).position().metres();
        world.simulate(mgv::physics::Duration{step});
        flight.seconds += step;
        const auto position = world.body(ball).position().metres();
        flight.apex = std::max(flight.apex, position.y);
        if (position.y < 0.0F) {
            break;
        }
    }
    const auto velocity = world.body(ball).linear_velocity().metres_per_second();
    flight.carry = previous.x;
    flight.descent_degrees = std::atan2(-velocity.y, velocity.x) / degrees_to_radians;
    flight.retained_spin =
        world.body(ball).angular_velocity().radians_per_second().z / spin;
    return flight;
}

} // namespace

TEST_CASE("a tour driver, 7 iron and wedge fly the shape they should") {
    // Launch conditions are TrackMan's published PGA Tour averages. The
    // reference carry, apex and descent angle are theirs too; what this
    // solver produces is pinned alongside, so that a change to the
    // coefficients has to be re-measured against the same yardstick rather
    // than quietly moving it.
    //
    // | club   | carry (m)   | apex (m)    | descent (deg) |
    // |        | tour / here | tour / here | tour  / here  |
    // | driver | 251 / 227   |  31 / 33.7  |   38  / 41.6  |
    // | 7 iron | 157 / 163   |  32 / 33.1  |   50  / 45.9  |
    // | wedge  | 124 / 127   |  32 / 34.1  |   52  / 49.8  |
    //
    // The irons and the wedge land within a few per cent on every measure.
    // The driver carries about ten per cent short while reaching the right
    // height, which says its drag is too high late in the flight rather than
    // its lift being wrong -- and a driver is the one club that spends its
    // whole flight above Re = 200,000, where Smits and Smith reported a
    // second fall in drag that this fit does not carry. That is the next
    // thing to do to this model, and the number to beat is 251.

    const auto driver = struck(74.65F, 10.9F, 2'686.0F);
    CHECK(driver.carry == Catch::Approx(227.2F).margin(2.0F));
    CHECK(driver.apex == Catch::Approx(33.7F).margin(1.0F));
    CHECK(driver.descent_degrees == Catch::Approx(41.6F).margin(1.0F));

    const auto seven_iron = struck(53.64F, 16.3F, 7'124.0F);
    CHECK(seven_iron.carry == Catch::Approx(162.6F).margin(2.0F));
    CHECK(seven_iron.apex == Catch::Approx(33.1F).margin(1.0F));
    CHECK(seven_iron.descent_degrees == Catch::Approx(45.9F).margin(1.0F));

    const auto wedge = struck(45.6F, 24.2F, 9'304.0F);
    CHECK(wedge.carry == Catch::Approx(127.3F).margin(2.0F));
    CHECK(wedge.apex == Catch::Approx(34.1F).margin(1.0F));
    CHECK(wedge.descent_degrees == Catch::Approx(49.8F).margin(1.0F));

    // The shape that makes a wedge a wedge: it goes no higher than the
    // driver, and it comes down far steeper.
    CHECK(wedge.descent_degrees > driver.descent_degrees + 5.0F);
}

TEST_CASE("spin buys height and a steeper landing") {
    // Independent of how well the coefficients are calibrated: more spin must
    // mean more lift, and more lift on the same strike must mean a higher
    // apex and a steeper descent. This is what a constant lift coefficient
    // could not express.
    const auto quiet = struck(60.0F, 14.0F, 2'000.0F);
    const auto lively = struck(60.0F, 14.0F, 6'000.0F);

    CHECK(lively.apex > quiet.apex);
    CHECK(lively.descent_degrees > quiet.descent_degrees);
    CHECK(lively.seconds > quiet.seconds);
}

TEST_CASE("a course a mile up plays longer") {
    // Air density is most of what altitude does to a golf ball, and it is
    // already live: the weather poll feeds it. Denver is about 0.86 of sea
    // level density.
    const auto sea_level = struck(74.65F, 10.9F, 2'686.0F, 1.225F);
    const auto altitude = struck(74.65F, 10.9F, 2'686.0F, 1.054F);

    CHECK(altitude.carry > sea_level.carry);
    // Thinner air holds spin for longer as well, which is the part a model
    // with a fixed decay rate would miss.
    CHECK(altitude.retained_spin > sea_level.retained_spin);
}
