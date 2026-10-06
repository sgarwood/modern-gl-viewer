#include "mgv/physics/collider.hpp"
#include "mgv/physics/rigid_body.hpp"

#include <catch2/catch_test_macros.hpp>

#include <stdexcept>
#include <variant>

TEST_CASE("collider value objects validate sphere and box dimensions") {
    const auto sphere = mgv::physics::Collider::sphere(mgv::physics::Length{2.0F});
    const auto box = mgv::physics::Collider::box(
        mgv::physics::Dimensions{{1.0F, 2.0F, 3.0F}});

    CHECK(std::get<mgv::physics::SphereCollider>(sphere.shape()).radius.metres() == 2.0F);
    CHECK(std::get<mgv::physics::BoxCollider>(box.shape()).half_extents.metres() ==
          mgv::Vec3{1.0F, 2.0F, 3.0F});
    CHECK_THROWS_AS(
        mgv::physics::Collider::sphere(mgv::physics::Length{0.0F}),
        std::invalid_argument);
    CHECK_THROWS_AS(
        mgv::physics::Collider::box(mgv::physics::Dimensions{{1.0F, 0.0F, 1.0F}}),
        std::invalid_argument);
}

TEST_CASE("heightmap colliders validate their grid and samples") {
    const auto collider = mgv::physics::Collider::heightmap(
        2, 3, 0.5F, 1.0F, {0.0F, 0.1F, 0.2F, 0.3F, 0.4F, 0.5F});
    const auto& heightmap = std::get<mgv::physics::HeightmapCollider>(collider.shape());

    CHECK(heightmap.width == 2);
    CHECK(heightmap.depth == 3);
    CHECK(heightmap.heights.size() == 6);
    CHECK_THROWS_AS(
        mgv::physics::Collider::heightmap(1, 2, 1.0F, 1.0F, {0.0F, 0.0F}),
        std::invalid_argument);
    CHECK_THROWS_AS(
        mgv::physics::Collider::heightmap(2, 2, 1.0F, 1.0F, {0.0F}),
        std::invalid_argument);
}

TEST_CASE("rigid body builder creates validated body definitions") {
    const auto definition = mgv::physics::RigidBodyBuilder{
        mgv::physics::Collider::sphere(mgv::physics::Length{0.5F})}
                                .motion(mgv::physics::MotionType::dynamic)
                                .at(mgv::physics::Position{{1.0F, 2.0F, 3.0F}})
                                .velocity(mgv::physics::LinearVelocity{{4.0F, 0.0F, 0.0F}})
                                .mass(mgv::physics::Mass{2.0F})
                                .restitution(0.75F)
                                .build();

    CHECK(definition.motion() == mgv::physics::MotionType::dynamic);
    CHECK(definition.position().metres() == mgv::Vec3{1.0F, 2.0F, 3.0F});
    CHECK(definition.velocity().metres_per_second() == mgv::Vec3{4.0F, 0.0F, 0.0F});
    CHECK(definition.mass().kilograms() == 2.0F);
    CHECK(definition.restitution() == 0.75F);
    CHECK_THROWS_AS(
        mgv::physics::RigidBodyBuilder{
            mgv::physics::Collider::sphere(mgv::physics::Length{1.0F})}
            .restitution(1.1F),
        std::invalid_argument);
}
