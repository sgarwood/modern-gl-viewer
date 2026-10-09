#include "mgv/physics/physics_world.hpp"
#include "mgv/physics/rigid_body.hpp"
#include "mgv/physics/terrain_material.hpp"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <cmath>
#include <vector>

namespace {

constexpr float ball_radius = 0.021335F;
constexpr float ball_mass = 0.04593F;
constexpr float step = 1.0F / 60.0F;
constexpr int grid = 21;

/// Ground falling away at `grade` along -X, as a heightmap, because a tilted
/// contact normal is the whole point and a box only ever has axis-aligned
/// ones. Centred on the origin, so a ball at x = 0 sits in the middle of it.
[[nodiscard]] mgv::physics::RigidBodyDefinition slope(float grade, float rolling_resistance) {
    std::vector<float> heights(static_cast<std::size_t>(grid) * grid);
    for (int z = 0; z < grid; ++z) {
        for (int x = 0; x < grid; ++x) {
            heights[static_cast<std::size_t>(z) * grid + x] =
                grade * static_cast<float>(x);
        }
    }
    return mgv::physics::RigidBodyBuilder{
        mgv::physics::Collider::heightmap(grid, grid, 1.0F, 1.0F, std::move(heights))}
        .motion(mgv::physics::MotionType::static_body)
        .at(mgv::physics::Position{{-10.0F, 0.0F, -10.0F}})
        .material(mgv::physics::TerrainMaterial{
            .dynamic_friction = 0.4F,
            .rolling_resistance = rolling_resistance,
        })
        .build();
}

[[nodiscard]] mgv::physics::RigidBodyDefinition ball_at(mgv::Vec3 position, mgv::Vec3 velocity = {}) {
    return mgv::physics::RigidBodyBuilder{
        mgv::physics::Collider::sphere(mgv::physics::Length{ball_radius})}
        .at(mgv::physics::Position{position})
        .velocity(mgv::physics::LinearVelocity{velocity})
        .mass(mgv::physics::Mass{ball_mass})
        .material(mgv::physics::TerrainMaterial{.rolling_resistance = 0.0F})
        .build();
}

[[nodiscard]] mgv::physics::PhysicsConfiguration configured() {
    mgv::physics::PhysicsConfiguration configuration;
    configuration.fixed_time_step = mgv::physics::Duration{step};
    configuration.gravity = mgv::physics::Acceleration{{0.0F, -9.81F, 0.0F}};
    return configuration;
}

void run_for(mgv::physics::PhysicsWorld& world, float seconds) {
    const auto ticks = static_cast<int>(std::lround(seconds / step));
    for (int tick = 0; tick < ticks; ++tick) {
        world.simulate(mgv::physics::Duration{step});
    }
}

} // namespace

TEST_CASE("a ball left on a slope stays where it was put") {
    // It used to walk downhill about a centimetre a second with its velocity
    // reading exactly zero: gravity re-penetrated the ground every step, and
    // the correction that pushed it back out ran along a tilted normal.
    // Friction had no answer, because there was no velocity to oppose.
    mgv::physics::PhysicsWorld world{configured()};
    static_cast<void>(world.add_body(slope(0.05F, mgv::physics::rolling_resistance_from_stimp(10.0F))));
    const auto ball = world.add_body(ball_at({0.0F, 0.5F + ball_radius, 0.0F}));

    run_for(world, 1.0F);
    const auto settled = world.body(ball).position().metres();
    run_for(world, 3.0F);
    const auto later = world.body(ball).position().metres();

    // Whatever it drifts while settling is bounded by the sleep delay, and
    // is under half a centimetre -- the width of a blade of grass.
    CHECK(std::abs(settled.x) < 0.005F);
    // And after that, nothing at all. This is the part that was unbounded.
    CHECK(later.x == Catch::Approx(settled.x));
    CHECK(later.y == Catch::Approx(settled.y));
    CHECK(world.body(ball).sleeping());
}

TEST_CASE("a ball a slope can keep rolling does not fall asleep") {
    // The guard on the sleep thresholds. A green at stimp ten cannot hold a
    // ball on a fifteen per cent fall -- gravity beats the rolling resistance
    // -- so the ball must still be moving when a flatter one would have
    // stopped. It passes the speed threshold inside a fiftieth of a second,
    // which is why the delay is set where it is.
    mgv::physics::PhysicsWorld world{configured()};
    static_cast<void>(world.add_body(slope(0.15F, mgv::physics::rolling_resistance_from_stimp(10.0F))));
    const auto ball = world.add_body(ball_at({0.0F, 1.5F + ball_radius, 0.0F}));

    run_for(world, 3.0F);

    CHECK_FALSE(world.body(ball).sleeping());
    CHECK(world.body(ball).position().metres().x < -0.5F);
}

TEST_CASE("striking a ball that has stopped wakes it") {
    mgv::physics::PhysicsWorld world{configured()};
    static_cast<void>(world.add_body(slope(0.0F, 0.0F)));
    const auto ball = world.add_body(ball_at({0.0F, ball_radius, 0.0F}));

    run_for(world, 1.0F);
    REQUIRE(world.body(ball).sleeping());

    world.set_velocity(
        ball,
        mgv::physics::LinearVelocity{{1.0F, 0.0F, 0.0F}},
        mgv::physics::AngularVelocity{{0.0F, 0.0F, -1.0F / ball_radius}});
    CHECK_FALSE(world.body(ball).sleeping());

    run_for(world, 0.25F);
    CHECK(world.body(ball).position().metres().x > 0.1F);
}

TEST_CASE("a ball rolled into a sleeping one wakes it") {
    mgv::physics::PhysicsWorld world{configured()};
    static_cast<void>(world.add_body(slope(0.0F, 0.0F)));
    const auto resting = world.add_body(ball_at({0.3F, ball_radius, 0.0F}));
    const auto rolling = world.add_body(
        ball_at({0.0F, ball_radius, 0.0F}, {0.5F, 0.0F, 0.0F}));

    run_for(world, 0.6F);
    REQUIRE(world.body(resting).sleeping());
    REQUIRE_FALSE(world.body(rolling).sleeping());

    // Watched for rather than sampled at the end: once awake it is struck,
    // rolls, and comes to rest again, so a single look a second later says
    // nothing about whether it ever moved.
    auto woke = false;
    for (int tick = 0; tick < 120 && !woke; ++tick) {
        run_for(world, step);
        woke = !world.body(resting).sleeping();
    }
    REQUIRE(woke);

    const auto struck_from = world.body(resting).position().metres().x;
    run_for(world, 0.25F);
    CHECK(world.body(resting).position().metres().x > struck_from);
}
