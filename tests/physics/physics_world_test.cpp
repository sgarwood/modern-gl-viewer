#include "mgv/physics/physics_world.hpp"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <cmath>
#include <memory>
#include <optional>
#include <stdexcept>
#include <vector>

namespace {

[[nodiscard]] mgv::physics::RigidBodyDefinition sphere(
    mgv::physics::MotionType motion,
    mgv::Vec3 position,
    mgv::Vec3 velocity = {}) {
    return mgv::physics::RigidBodyBuilder{
        mgv::physics::Collider::sphere(mgv::physics::Length{1.0F})}
        .motion(motion)
        .at(mgv::physics::Position{position})
        .velocity(mgv::physics::LinearVelocity{velocity})
        .restitution(1.0F)
        .build();
}

class CountingDetector final : public mgv::physics::CollisionDetector {
public:
    explicit CountingDetector(int& calls) : calls_{calls} {}

    [[nodiscard]] std::optional<mgv::physics::ContactManifold> detect(
        const mgv::physics::RigidBody&,
        const mgv::physics::RigidBody&) const override {
        ++calls_;
        return std::nullopt;
    }

private:
    int& calls_;
};

} // namespace

TEST_CASE("physics world uses an injected collision detector domain service") {
    int calls{};
    mgv::physics::PhysicsConfiguration configuration;
    configuration.fixed_time_step = mgv::physics::Duration{0.1F};
    configuration.gravity = mgv::physics::Acceleration{{}};
    mgv::physics::PhysicsWorld world{
        configuration,
        std::make_unique<CountingDetector>(calls),
    };
    static_cast<void>(world.add_body(sphere(mgv::physics::MotionType::static_body, {})));
    static_cast<void>(world.add_body(sphere(mgv::physics::MotionType::dynamic, {3.0F, 0.0F, 0.0F})));

    world.simulate(mgv::physics::Duration{0.1F});

    CHECK(calls == 1);
}

TEST_CASE("physics world advances dynamic bodies on a fixed timestep") {
    mgv::physics::PhysicsConfiguration configuration;
    configuration.fixed_time_step = mgv::physics::Duration{0.1F};
    configuration.gravity = mgv::physics::Acceleration{{0.0F, -10.0F, 0.0F}};
    mgv::physics::PhysicsWorld world{configuration};
    const auto id = world.add_body(sphere(mgv::physics::MotionType::dynamic, {}));

    world.simulate(mgv::physics::Duration{0.05F});
    CHECK(world.body(id).position().metres().y == Catch::Approx(0.0F));
    world.simulate(mgv::physics::Duration{0.05F});

    CHECK(world.body(id).linear_velocity().metres_per_second().y == Catch::Approx(-1.0F));
    CHECK(world.body(id).position().metres().y == Catch::Approx(-0.1F));
}

TEST_CASE("physics world resolves penetration and restitution impulses") {
    mgv::physics::PhysicsConfiguration configuration;
    configuration.fixed_time_step = mgv::physics::Duration{0.01F};
    configuration.gravity = mgv::physics::Acceleration{{}};
    // Vacuum: this is about the restitution impulse, and a one metre sphere pushes
    // enough air aside to show up in the fourth decimal place.
    configuration.air_density = 0.0F;
    mgv::physics::PhysicsWorld world{configuration};
    const auto floor = world.add_body(sphere(mgv::physics::MotionType::static_body, {}));
    const auto falling = world.add_body(
        sphere(mgv::physics::MotionType::dynamic, {0.0F, 1.5F, 0.0F}, {0.0F, -1.0F, 0.0F}));

    world.simulate(mgv::physics::Duration{0.01F});

    CHECK(world.body(floor).position().metres().y == Catch::Approx(0.0F));
    CHECK(world.body(falling).position().metres().y == Catch::Approx(2.0F));
    CHECK(world.body(falling).linear_velocity().metres_per_second().y == Catch::Approx(1.0F));
    REQUIRE(world.collisions().size() == 1);
    CHECK(world.collisions().front().first == floor);
    CHECK(world.collisions().front().second == falling);
}

TEST_CASE("physics world rejects unknown body identifiers") {
    mgv::physics::PhysicsWorld world;

    CHECK_THROWS_AS(world.body(mgv::physics::BodyId{42}), std::out_of_range);
}

TEST_CASE("physics body handles are not aliased after removal") {
    mgv::physics::PhysicsWorld world;
    const auto removed = world.add_body(
        sphere(mgv::physics::MotionType::dynamic, {}));
    const auto retained = world.add_body(
        sphere(mgv::physics::MotionType::dynamic, {3.0F, 0.0F, 0.0F}));

    CHECK(world.remove_body(removed));
    CHECK_FALSE(world.contains(removed));
    CHECK(world.contains(retained));
    CHECK_THROWS_AS(world.body(removed), std::out_of_range);

    const auto replacement = world.add_body(
        sphere(mgv::physics::MotionType::dynamic, {6.0F, 0.0F, 0.0F}));
    CHECK(replacement != removed);
}

TEST_CASE("physics world applies strongly typed impulses only to dynamic bodies") {
    mgv::physics::PhysicsConfiguration configuration;
    configuration.gravity = mgv::physics::Acceleration{{}};
    mgv::physics::PhysicsWorld world{configuration};
    const auto dynamic = world.add_body(
        mgv::physics::RigidBodyBuilder{
            mgv::physics::Collider::sphere(mgv::physics::Length{1.0F})}
            .mass(mgv::physics::Mass{2.0F})
            .build());
    const auto stationary = world.add_body(
        sphere(mgv::physics::MotionType::static_body, {4.0F, 0.0F, 0.0F}));

    world.apply_impulse(dynamic, mgv::physics::Impulse{{2.0F, 0.0F, 0.0F}});
    world.apply_impulse(stationary, mgv::physics::Impulse{{2.0F, 0.0F, 0.0F}});

    CHECK(world.body(dynamic).linear_velocity().metres_per_second().x == Catch::Approx(1.0F));
    CHECK(world.body(stationary).linear_velocity().metres_per_second().x == Catch::Approx(0.0F));
}

TEST_CASE("fixed-step simulation is independent of frame chunking") {
    mgv::physics::PhysicsConfiguration configuration;
    configuration.fixed_time_step = mgv::physics::Duration{0.1F};
    configuration.gravity = mgv::physics::Acceleration{{0.0F, -10.0F, 0.0F}};
    mgv::physics::PhysicsWorld single_frame{configuration};
    mgv::physics::PhysicsWorld split_frames{configuration};
    const auto single_id = single_frame.add_body(
        sphere(mgv::physics::MotionType::dynamic, {0.0F, 10.0F, 0.0F}));
    const auto split_id = split_frames.add_body(
        sphere(mgv::physics::MotionType::dynamic, {0.0F, 10.0F, 0.0F}));

    single_frame.simulate(mgv::physics::Duration{0.3F});
    split_frames.simulate(mgv::physics::Duration{0.1F});
    split_frames.simulate(mgv::physics::Duration{0.1F});
    split_frames.simulate(mgv::physics::Duration{0.1F});

    CHECK(single_frame.body(single_id).position().metres().y ==
          Catch::Approx(split_frames.body(split_id).position().metres().y));
    CHECK(single_frame.body(single_id).linear_velocity().metres_per_second().y ==
          Catch::Approx(split_frames.body(split_id).linear_velocity().metres_per_second().y));
}

TEST_CASE("physics world validates its strategy and stepping policy") {
    mgv::physics::PhysicsConfiguration invalid_step;
    invalid_step.fixed_time_step = mgv::physics::Duration{0.0F};
    CHECK_THROWS_AS(mgv::physics::PhysicsWorld{invalid_step}, std::invalid_argument);

    mgv::physics::PhysicsConfiguration configuration;
    CHECK_THROWS_AS(
        (mgv::physics::PhysicsWorld{
            configuration,
            std::unique_ptr<mgv::physics::CollisionDetector>{},
        }),
        std::invalid_argument);
}

TEST_CASE("friction slows a ball sliding across level ground") {
    mgv::physics::PhysicsConfiguration configuration;
    configuration.fixed_time_step = mgv::physics::Duration{1.0F / 120.0F};
    mgv::physics::PhysicsWorld world{configuration};

    constexpr float radius = 0.021335F;
    const auto ball = world.add_body(
        mgv::physics::RigidBodyBuilder{
            mgv::physics::Collider::sphere(mgv::physics::Length{radius})}
            .at(mgv::physics::Position{{0.0F, radius, 0.0F}})
            .mass(mgv::physics::Mass{0.04593F})
            .velocity(mgv::physics::LinearVelocity{{4.0F, 0.0F, 0.0F}})
            .restitution(0.0F)
            .build());
    static_cast<void>(world.add_body(
        mgv::physics::RigidBodyBuilder{
            mgv::physics::Collider::box(mgv::physics::Dimensions{{50.0F, 0.5F, 50.0F}})}
            .motion(mgv::physics::MotionType::static_body)
            .at(mgv::physics::Position{{0.0F, -0.5F, 0.0F}})
            .build()));

    const auto speed_of = [&world, ball] {
        const auto velocity = world.body(ball).linear_velocity().metres_per_second();
        return std::sqrt(velocity.x * velocity.x + velocity.z * velocity.z);
    };
    const auto initial_speed = speed_of();

    for (int step = 0; step < 120; ++step) {
        world.simulate(mgv::physics::Duration{1.0F / 120.0F});
    }

    // Contact friction can only ever remove kinetic energy. Applying the
    // impulse with the wrong sign drives the ball instead of retarding it,
    // and a ball resting on any slope then accelerates without bound.
    CHECK(speed_of() < initial_speed);
}

TEST_CASE("a ball left on a slope does not accelerate without bound") {
    mgv::physics::PhysicsConfiguration configuration;
    configuration.fixed_time_step = mgv::physics::Duration{1.0F / 120.0F};
    mgv::physics::PhysicsWorld world{configuration};

    // A five percent grade, which is about what a fairway falls.
    constexpr int samples = 64;
    constexpr float spacing = 2.0F;
    std::vector<float> heights;
    heights.reserve(samples * samples);
    for (int row = 0; row < samples; ++row) {
        for (int column = 0; column < samples; ++column) {
            heights.push_back(-0.05F * static_cast<float>(row) * spacing);
        }
    }

    constexpr float radius = 0.021335F;
    const auto start_x = 32.0F;
    const auto start_z = 32.0F;
    const auto ball = world.add_body(
        mgv::physics::RigidBodyBuilder{
            mgv::physics::Collider::sphere(mgv::physics::Length{radius})}
            .at(mgv::physics::Position{{start_x, -0.05F * start_z + radius, start_z}})
            .mass(mgv::physics::Mass{0.04593F})
            .restitution(0.3F)
            .build());
    static_cast<void>(world.add_body(
        mgv::physics::RigidBodyBuilder{
            mgv::physics::Collider::heightmap(
                samples, samples, spacing, spacing, std::move(heights))}
            .motion(mgv::physics::MotionType::static_body)
            .at(mgv::physics::Position{{0.0F, 0.0F, 0.0F}})
            .build()));

    for (int step = 0; step < 600; ++step) {   // five seconds
        world.simulate(mgv::physics::Duration{1.0F / 120.0F});
    }

    const auto velocity = world.body(ball).linear_velocity().metres_per_second();
    const auto speed = std::sqrt(
        velocity.x * velocity.x + velocity.y * velocity.y + velocity.z * velocity.z);
    const auto travelled = std::abs(world.body(ball).position().metres().z - start_z);

    // Free sliding down a five percent grade for five seconds reaches about
    // two and a half metres per second and six metres. Anything far beyond
    // that is energy the solver invented.
    CHECK(speed < 4.0F);
    CHECK(travelled < 12.0F);
}

namespace {

/// A heightmap of constant height, as a patch that can be placed anywhere.
[[nodiscard]] mgv::physics::Collider flat_patch(int samples, float spacing, float height) {
    return mgv::physics::Collider::heightmap(
        samples, samples, spacing, spacing,
        std::vector<float>(static_cast<std::size_t>(samples * samples), height));
}

} // namespace

TEST_CASE("a static collider can be replaced so the ground follows the ball") {
    mgv::physics::PhysicsConfiguration configuration;
    configuration.fixed_time_step = mgv::physics::Duration{1.0F / 120.0F};
    mgv::physics::PhysicsWorld world{configuration};

    constexpr float radius = 0.021335F;
    constexpr int samples = 21;
    constexpr float spacing = 1.0F;

    const auto ball = world.add_body(
        mgv::physics::RigidBodyBuilder{
            mgv::physics::Collider::sphere(mgv::physics::Length{radius})}
            .at(mgv::physics::Position{{0.0F, radius, 0.0F}})
            .mass(mgv::physics::Mass{0.04593F})
            .restitution(0.0F)
            .build());
    // A twenty metre patch centred on the origin.
    const auto ground = world.add_body(
        mgv::physics::RigidBodyBuilder{flat_patch(samples, spacing, 0.0F)}
            .motion(mgv::physics::MotionType::static_body)
            .at(mgv::physics::Position{{-10.0F, 0.0F, -10.0F}})
            .build());

    // Long enough for a ball to fall a few metres and come to rest.
    const auto settle = [&world] {
        for (int step = 0; step < 360; ++step) {
            world.simulate(mgv::physics::Duration{1.0F / 120.0F});
        }
    };
    settle();
    CHECK(world.body(ball).position().metres().y == Catch::Approx(radius).margin(0.005F));

    SECTION("a ball beyond the patch has nothing to rest on") {
        world.set_position(ball, mgv::physics::Position{{400.0F, radius, 0.0F}});
        settle();
        CHECK(world.body(ball).position().metres().y < 0.0F);
    }

    SECTION("moving the patch under it gives it ground again") {
        world.set_position(ball, mgv::physics::Position{{400.0F, radius, 0.0F}});
        world.set_static_collider(
            ground,
            flat_patch(samples, spacing, 0.0F),
            mgv::physics::Position{{390.0F, 0.0F, -10.0F}});
        settle();
        CHECK(world.body(ball).position().metres().y == Catch::Approx(radius).margin(0.005F));
    }

    SECTION("the replacement carries its own heights") {
        world.set_static_collider(
            ground,
            flat_patch(samples, spacing, 5.0F),
            mgv::physics::Position{{-10.0F, 0.0F, -10.0F}});
        world.set_position(ball, mgv::physics::Position{{0.0F, 8.0F, 0.0F}});
        settle();
        CHECK(world.body(ball).position().metres().y == Catch::Approx(5.0F + radius).margin(0.01F));
    }
}

TEST_CASE("only a static body's collider may be replaced") {
    mgv::physics::PhysicsWorld world;
    const auto ball = world.add_body(
        mgv::physics::RigidBodyBuilder{
            mgv::physics::Collider::sphere(mgv::physics::Length{0.5F})}
            .build());

    CHECK_THROWS_AS(
        world.set_static_collider(ball, flat_patch(4, 1.0F, 0.0F), mgv::physics::Position{}),
        std::logic_error);
    CHECK_THROWS_AS(
        world.set_static_collider(
            mgv::physics::BodyId{9999}, flat_patch(4, 1.0F, 0.0F), mgv::physics::Position{}),
        std::out_of_range);
}
