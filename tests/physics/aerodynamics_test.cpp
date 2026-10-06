#include "mgv/physics/physics_world.hpp"
#include "mgv/physics/units.hpp"
#include "mgv/physics/rigid_body.hpp"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

namespace {
    mgv::physics::RigidBodyDefinition golf_ball(mgv::Vec3 position, mgv::Vec3 velocity, mgv::Vec3 angular_velocity) {
        return mgv::physics::RigidBodyBuilder{
            mgv::physics::Collider::sphere(mgv::physics::Length{0.02135F})} // standard golf ball radius
            .motion(mgv::physics::MotionType::dynamic)
            .at(mgv::physics::Position{position})
            .velocity(mgv::physics::LinearVelocity{velocity})
            .angular_velocity(mgv::physics::AngularVelocity{angular_velocity}) // THIS WILL NOT COMPILE YET
            .mass(mgv::physics::Mass{0.04593F}) // 45.93 grams
            .build();
    }
}

TEST_CASE("golf ball with backspin generates lift (magnus effect)") {
    mgv::physics::PhysicsConfiguration configuration;
    configuration.fixed_time_step = mgv::physics::Duration{0.01F};
    configuration.gravity = mgv::physics::Acceleration{{0.0F, -9.81F, 0.0F}};
    mgv::physics::PhysicsWorld world{configuration};
    
    // Ball 1: No spin
    const auto no_spin = world.add_body(golf_ball({0.0F, 0.0F, 0.0F}, {50.0F, 0.0F, 0.0F}, {0.0F, 0.0F, 0.0F}));
    // Positive Z angular velocity is backspin for travel along +X in the
    // engine's right-handed, Y-up coordinate system.
    const auto backspin = world.add_body(golf_ball(
        {0.0F, 0.0F, 0.0F},
        {50.0F, 0.0F, 0.0F},
        {0.0F, 0.0F, 300.0F}));

    world.simulate(mgv::physics::Duration{0.1F});

    // The backspin ball should have a higher Y position than the no_spin ball due to lift
    CHECK(world.body(backspin).position().metres().y > world.body(no_spin).position().metres().y);
}

TEST_CASE("wind alters trajectory") {
    mgv::physics::PhysicsConfiguration configuration;
    configuration.fixed_time_step = mgv::physics::Duration{0.01F};
    configuration.gravity = mgv::physics::Acceleration{{0.0F, -9.81F, 0.0F}};
    
    // Create world with NO wind
    mgv::physics::PhysicsWorld world_no_wind{configuration};
    const auto ball_no_wind = world_no_wind.add_body(golf_ball({0.0F, 0.0F, 0.0F}, {50.0F, 0.0F, 0.0F}, {0.0F, 0.0F, 0.0F}));
    
    // Create world WITH strong crosswind
    mgv::physics::PhysicsWorld world_with_wind{configuration};
    world_with_wind.set_wind(mgv::physics::LinearVelocity{{0.0F, 0.0F, 20.0F}}); // 20 m/s crosswind in Z
    const auto ball_with_wind = world_with_wind.add_body(golf_ball({0.0F, 0.0F, 0.0F}, {50.0F, 0.0F, 0.0F}, {0.0F, 0.0F, 0.0F}));

    world_no_wind.simulate(mgv::physics::Duration{0.5F});
    world_with_wind.simulate(mgv::physics::Duration{0.5F});

    // The ball in the wind should have been blown in the Z direction
    CHECK(world_with_wind.body(ball_with_wind).position().metres().z > world_no_wind.body(ball_no_wind).position().metres().z);
    
    // The headwind component (if we added one) would slow down X.
    // We added a crosswind, so the drag force in Z will push it into Z.
}
