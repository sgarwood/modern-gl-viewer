#include "mgv/physics/physics_world.hpp"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <memory>
#include <optional>
#include <stdexcept>

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
