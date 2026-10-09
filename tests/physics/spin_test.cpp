#include "mgv/physics/physics_world.hpp"
#include "mgv/physics/rigid_body.hpp"
#include "mgv/physics/terrain_material.hpp"
#include "mgv/physics/units.hpp"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <cmath>

namespace {

constexpr float ball_radius = 0.021335F;
constexpr float ball_mass = 0.04593F;
constexpr float rpm_to_radians_per_second = 0.104719755F;
constexpr float fine_step = 1.0F / 240.0F;

/// A ball with no rolling resistance of its own.
///
/// The solver takes the greater of the two materials in a contact, so a ball
/// carrying the default would put a floor under every green's speed.
[[nodiscard]] mgv::physics::RigidBodyDefinition golf_ball(
    mgv::Vec3 position,
    mgv::Vec3 velocity,
    mgv::Vec3 spin) {
    return mgv::physics::RigidBodyBuilder{
        mgv::physics::Collider::sphere(mgv::physics::Length{ball_radius})}
        .motion(mgv::physics::MotionType::dynamic)
        .at(mgv::physics::Position{position})
        .velocity(mgv::physics::LinearVelocity{velocity})
        .angular_velocity(mgv::physics::AngularVelocity{spin})
        .mass(mgv::physics::Mass{ball_mass})
        .material(mgv::physics::TerrainMaterial{.rolling_resistance = 0.0F})
        .build();
}

/// Ground whose top face is the plane y = 0.
[[nodiscard]] mgv::physics::RigidBodyDefinition turf(
    float dynamic_friction,
    float rolling_resistance) {
    return mgv::physics::RigidBodyBuilder{
        mgv::physics::Collider::box(mgv::physics::Dimensions{{60.0F, 1.0F, 60.0F}})}
        .motion(mgv::physics::MotionType::static_body)
        .at(mgv::physics::Position{{0.0F, -1.0F, 0.0F}})
        .material(mgv::physics::TerrainMaterial{
            .dynamic_friction = dynamic_friction,
            .rolling_resistance = rolling_resistance,
        })
        .build();
}

/// A world with air in it, or without.
[[nodiscard]] mgv::physics::PhysicsConfiguration configured(
    float step,
    float air_density,
    float gravity = -9.81F) {
    mgv::physics::PhysicsConfiguration configuration;
    configuration.fixed_time_step = mgv::physics::Duration{step};
    configuration.gravity = mgv::physics::Acceleration{{0.0F, gravity, 0.0F}};
    configuration.air_density = air_density;
    return configuration;
}

/// Runs the world for `seconds`, one fixed step per call.
///
/// `PhysicsWorld::simulate` will only catch up `maximum_substeps` steps in a
/// single call and silently drops the rest, so handing it a whole second of
/// elapsed time runs eight steps and returns. Every test here cares about
/// where a ball ends up seconds later, which makes that the difference
/// between measuring the solver and measuring the accumulator.
void run_for(mgv::physics::PhysicsWorld& world, float step, float seconds) {
    const auto ticks = static_cast<int>(std::lround(seconds / step));
    for (int tick = 0; tick < ticks; ++tick) {
        world.simulate(mgv::physics::Duration{step});
    }
}

/// The speed of the ball's surface where it touches ground below it. Zero is
/// the no-slip condition: the ball is rolling rather than skidding.
[[nodiscard]] float slip_along_x(const mgv::physics::RigidBody& ball) {
    return ball.linear_velocity().metres_per_second().x +
        ball.angular_velocity().radians_per_second().z * ball_radius;
}

} // namespace

TEST_CASE("a sliding ball sheds five sevenths of its speed into its spin") {
    // The textbook result for a sphere launched sliding without spin: friction
    // at the contact takes two sevenths of the momentum into rotation and the
    // ball rolls away on the remaining five. It is worth pinning exactly,
    // because it is the one number in the contact that is not a matter of
    // taste -- it follows from 2/5 m r^2 and nothing else.
    mgv::physics::PhysicsWorld world{configured(fine_step, 0.0F)};
    static_cast<void>(world.add_body(turf(0.5F, 0.0F)));
    const auto ball = world.add_body(
        golf_ball({0.0F, ball_radius, 0.0F}, {2.0F, 0.0F, 0.0F}, {}));

    run_for(world, fine_step, 1.0F);

    const auto& body = world.body(ball);
    CHECK(body.linear_velocity().metres_per_second().x ==
          Catch::Approx(2.0F * 5.0F / 7.0F).margin(0.01F));
    CHECK(slip_along_x(body) == Catch::Approx(0.0F).margin(0.01F));
}

TEST_CASE("enough backspin brings a ball back toward the player") {
    // A wedge that lands with little forward speed and a lot of spin left on
    // it walks backwards. The threshold is spin * r > 2.5 * speed, which at a
    // couple of metres per second is a spin a real wedge shot arrives with.
    const auto run = [](float spin_z) {
        mgv::physics::PhysicsWorld world{configured(fine_step, 0.0F)};
        static_cast<void>(world.add_body(turf(0.5F, 0.0F)));
        const auto ball = world.add_body(golf_ball(
            {0.0F, ball_radius, 0.0F}, {2.0F, 0.0F, 0.0F}, {0.0F, 0.0F, spin_z}));
        run_for(world, fine_step, 1.5F);
        return world.body(ball).linear_velocity().metres_per_second().x;
    };

    // (2.5 * 2 - 400 * r) / 3.5
    CHECK(run(400.0F) == Catch::Approx(-1.010F).margin(0.05F));
    // The same strike without the spin runs on forwards instead.
    CHECK(run(0.0F) > 1.0F);
}

TEST_CASE("a putt runs the length a stimpmeter says it should") {
    // The calibration this whole contact model is worth having for: a green
    // described by the number a greenkeeper measures, and a solver that
    // agrees with it. A stimpmeter releases the ball at 1.83 m/s, and ten
    // feet of run is a typical members' green.
    //
    // In vacuum, because `rolling_resistance_from_stimp` is derived without
    // drag; a real stimpmeter rolls in air, which costs a few centimetres.
    const auto roll_out = [](float stimp) {
        mgv::physics::PhysicsWorld world{configured(fine_step, 0.0F)};
        static_cast<void>(world.add_body(
            turf(0.4F, mgv::physics::rolling_resistance_from_stimp(stimp))));
        const auto speed = 1.829F;
        const auto ball = world.add_body(golf_ball(
            {0.0F, ball_radius, 0.0F},
            {speed, 0.0F, 0.0F},
            {0.0F, 0.0F, -speed / ball_radius}));

        for (int tick = 0; tick < 400; ++tick) {
            run_for(world, fine_step, 0.05F);
            if (std::abs(world.body(ball).linear_velocity().metres_per_second().x) <
                1.0e-4F) {
                break;
            }
        }
        return world.body(ball).position().metres().x;
    };

    // Ten feet is 3.048 m.
    CHECK(roll_out(10.0F) == Catch::Approx(3.048F).margin(0.08F));
    // Roll-out is proportional to the green speed, so a tournament green runs
    // the extra distance its extra feet promise and not some other distance.
    CHECK(roll_out(13.0F) / roll_out(8.0F) == Catch::Approx(13.0F / 8.0F).margin(0.05F));
}

TEST_CASE("spin bleeds off through a flight") {
    // Smits and Smith's measured spin-down rate puts a drive launched at
    // 2700 rpm at about 93 per cent of it six seconds later. Without any
    // decay the ball lifts the whole way down and arrives on the green with
    // a tee shot's spin still to spend.
    mgv::physics::PhysicsWorld world{configured(fine_step, 1.225F)};
    const auto launch_spin = 2'700.0F * rpm_to_radians_per_second;
    const auto ball = world.add_body(
        golf_ball({}, {60.0F, 25.0F, 0.0F}, {0.0F, 0.0F, launch_spin}));

    run_for(world, fine_step, 6.0F);

    const auto retained =
        world.body(ball).angular_velocity().radians_per_second().z / launch_spin;
    CHECK(retained == Catch::Approx(0.926F).margin(0.01F));
}

TEST_CASE("spin decay cannot carry a ball through zero") {
    // The reason the decay is a rate and not a fixed moment. A moment that
    // does not vanish with the spin drives a lightly spinning ball straight
    // through zero and out the other side, where its lift then points the
    // wrong way. Deliberately coarse steps: nothing here may depend on the
    // clock being fine.
    mgv::physics::PhysicsWorld world{configured(0.5F, 1.225F, 0.0F)};
    const auto ball = world.add_body(golf_ball({}, {70.0F, 0.0F, 0.0F}, {0.0F, 0.0F, 0.5F}));

    run_for(world, 0.5F, 20.0F);

    const auto spin = world.body(ball).angular_velocity().radians_per_second().z;
    CHECK(spin > 0.0F);
    CHECK(spin < 0.5F);
}

TEST_CASE("still air takes nothing off the spin") {
    // The decay is driven by airspeed, so a ball going nowhere keeps its
    // spin however long it is left. Also guards the wind term: the rate has
    // to come from the air moving over the ball, not from the clock.
    mgv::physics::PhysicsWorld world{configured(fine_step, 1.225F, 0.0F)};
    const auto ball = world.add_body(golf_ball({}, {}, {0.0F, 0.0F, 300.0F}));

    run_for(world, fine_step, 1.0F);

    CHECK(world.body(ball).angular_velocity().radians_per_second().z ==
          Catch::Approx(300.0F));
}
