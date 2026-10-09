#include "mgv/physics/physics_world.hpp"
#include "mgv/physics/rigid_body.hpp"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <cmath>

namespace {

constexpr float step = 1.0F / 240.0F;

[[nodiscard]] mgv::physics::RigidBodyDefinition golf_ball(
    mgv::Vec3 position, mgv::Vec3 velocity) {
    return mgv::physics::RigidBodyBuilder{
        mgv::physics::Collider::sphere(mgv::physics::Length{0.021335F})}
        .at(mgv::physics::Position{position})
        .velocity(mgv::physics::LinearVelocity{velocity})
        .mass(mgv::physics::Mass{0.04593F})
        .build();
}

[[nodiscard]] mgv::physics::PhysicsConfiguration still_air() {
    mgv::physics::PhysicsConfiguration configuration;
    configuration.fixed_time_step = mgv::physics::Duration{step};
    // No gravity: this is about the air, and a ball that drops out of the
    // volume under test is measuring something else.
    configuration.gravity = mgv::physics::Acceleration{{}};
    return configuration;
}

void run_for(mgv::physics::PhysicsWorld& world, float seconds) {
    const auto ticks = static_cast<int>(std::lround(seconds / step));
    for (int tick = 0; tick < ticks; ++tick) {
        world.simulate(mgv::physics::Duration{step});
    }
}

/// A crown eight metres across, centred on the origin at head height.
constexpr mgv::physics::FoliageVolume crown{
    .min = {-4.0F, 4.0F, -4.0F},
    .max = {4.0F, 12.0F, 4.0F},
    .density = 0.8F,
};

} // namespace

TEST_CASE("a ball through a canopy comes out slower") {
    const auto speed_after = [](bool planted) {
        mgv::physics::PhysicsWorld world{still_air()};
        if (planted) {
            world.add_foliage_volume(crown);
        }
        const auto ball = world.add_body(golf_ball({-8.0F, 8.0F, 0.0F}, {50.0F, 0.0F, 0.0F}));
        run_for(world, 0.32F);
        return world.body(ball).linear_velocity().metres_per_second().x;
    };

    // Through the crown, rather than past it at the same height.
    CHECK(speed_after(true) < speed_after(false) * 0.75F);
    // And it is slowed, not deflected somewhere: the old probabilistic
    // strike reversed the ball's vertical velocity on a draw from its own
    // coordinates.
    mgv::physics::PhysicsWorld world{still_air()};
    world.add_foliage_volume(crown);
    const auto ball = world.add_body(golf_ball({-8.0F, 8.0F, 0.0F}, {50.0F, 0.0F, 0.0F}));
    run_for(world, 0.32F);
    CHECK(world.body(ball).linear_velocity().metres_per_second().y == Catch::Approx(0.0F));
    CHECK(world.body(ball).linear_velocity().metres_per_second().z == Catch::Approx(0.0F));
}

TEST_CASE("a canopy shelters downwind whichever way the wind blows") {
    // The bug this replaced: shelter was tested against the volume's Z
    // bounds and called downwind, so it only ever worked for a wind blowing
    // along +Z. A crosswind sheltered nothing.
    const auto drift_along_wind = [](mgv::Vec3 wind, mgv::Vec3 ball_at, bool planted) {
        mgv::physics::PhysicsWorld world{still_air()};
        world.set_wind(mgv::physics::LinearVelocity{wind});
        if (planted) {
            world.add_foliage_volume(crown);
        }
        // A ball hanging in the wind, so what moves it is the wind alone.
        const auto ball = world.add_body(golf_ball(ball_at, {}));
        run_for(world, 1.0F);
        const auto moved = world.body(ball).position().metres();
        return std::sqrt(
            ((moved.x - ball_at.x) * (moved.x - ball_at.x)) +
            ((moved.z - ball_at.z) * (moved.z - ball_at.z)));
    };

    // Wind along +X: a ball ten metres downwind of the crown is sheltered.
    const mgv::Vec3 east{12.0F, 0.0F, 0.0F};
    CHECK(drift_along_wind(east, {10.0F, 8.0F, 0.0F}, true) <
          drift_along_wind(east, {10.0F, 8.0F, 0.0F}, false));
    // The same crown, the same distance, with the wind along +Z instead.
    const mgv::Vec3 north{0.0F, 0.0F, 12.0F};
    CHECK(drift_along_wind(north, {0.0F, 8.0F, 10.0F}, true) <
          drift_along_wind(north, {0.0F, 8.0F, 10.0F}, false));
    // Upwind of it, nothing is sheltered.
    CHECK(drift_along_wind(east, {-10.0F, 8.0F, 0.0F}, true) ==
          Catch::Approx(drift_along_wind(east, {-10.0F, 8.0F, 0.0F}, false)));
}
