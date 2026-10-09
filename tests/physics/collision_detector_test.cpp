#include "mgv/physics/collision_detector.hpp"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <cstdint>
#include <stdexcept>
#include <utility>

namespace {

[[nodiscard]] mgv::physics::RigidBody body(
    std::uint64_t id,
    mgv::physics::Collider collider,
    mgv::Vec3 position) {
    return mgv::physics::RigidBody{
        mgv::physics::BodyId{id},
        mgv::physics::RigidBodyBuilder{std::move(collider)}
            .at(mgv::physics::Position{position})
            .build(),
    };
}

} // namespace

TEST_CASE("contact manifolds enforce a normalized non-zero normal") {
    const mgv::physics::ContactManifold contact{
        {2.0F, 0.0F, 0.0F},
        mgv::physics::Length{0.25F},
    };

    CHECK(contact.normal() == mgv::Vec3{1.0F, 0.0F, 0.0F});
    CHECK_THROWS_AS(
        (mgv::physics::ContactManifold{{}, mgv::physics::Length{0.0F}}),
        std::invalid_argument);
}

TEST_CASE("discrete detector returns a sphere contact manifold") {
    const auto first = body(
        1, mgv::physics::Collider::sphere(mgv::physics::Length{1.0F}), {0.0F, 0.0F, 0.0F});
    const auto second = body(
        2, mgv::physics::Collider::sphere(mgv::physics::Length{1.0F}), {1.5F, 0.0F, 0.0F});

    const auto contact = mgv::physics::DiscreteCollisionDetector{}.detect(first, second);

    REQUIRE(contact.has_value());
    CHECK(contact->normal() == mgv::Vec3{1.0F, 0.0F, 0.0F});
    CHECK(contact->penetration().metres() == Catch::Approx(0.5F));
}

TEST_CASE("discrete detector separates non-overlapping spheres") {
    const auto first = body(
        1, mgv::physics::Collider::sphere(mgv::physics::Length{1.0F}), {0.0F, 0.0F, 0.0F});
    const auto second = body(
        2, mgv::physics::Collider::sphere(mgv::physics::Length{1.0F}), {3.0F, 0.0F, 0.0F});

    CHECK_FALSE(mgv::physics::DiscreteCollisionDetector{}.detect(first, second).has_value());
}

TEST_CASE("discrete detector handles box-box contacts on the shallowest axis") {
    const auto first = body(
        1,
        mgv::physics::Collider::box(mgv::physics::Dimensions{{1.0F, 1.0F, 1.0F}}),
        {0.0F, 0.0F, 0.0F});
    const auto second = body(
        2,
        mgv::physics::Collider::box(mgv::physics::Dimensions{{1.0F, 1.0F, 1.0F}}),
        {0.0F, 1.5F, 0.0F});

    const auto contact = mgv::physics::DiscreteCollisionDetector{}.detect(first, second);

    REQUIRE(contact.has_value());
    CHECK(contact->normal() == mgv::Vec3{0.0F, 1.0F, 0.0F});
    CHECK(contact->penetration().metres() == Catch::Approx(0.5F));
}

TEST_CASE("discrete detector handles sphere-box pairs in either order") {
    const auto sphere = body(
        1, mgv::physics::Collider::sphere(mgv::physics::Length{1.0F}), {0.0F, 1.5F, 0.0F});
    const auto box = body(
        2,
        mgv::physics::Collider::box(mgv::physics::Dimensions{{1.0F, 1.0F, 1.0F}}),
        {0.0F, 0.0F, 0.0F});
    const mgv::physics::DiscreteCollisionDetector detector;

    const auto sphere_first = detector.detect(sphere, box);
    const auto box_first = detector.detect(box, sphere);

    REQUIRE(sphere_first.has_value());
    REQUIRE(box_first.has_value());
    CHECK(sphere_first->normal() == mgv::Vec3{0.0F, -1.0F, 0.0F});
    CHECK(box_first->normal() == mgv::Vec3{0.0F, 1.0F, 0.0F});
    CHECK(sphere_first->penetration().metres() == Catch::Approx(0.5F));
    CHECK(box_first->penetration().metres() == Catch::Approx(0.5F));
}

TEST_CASE("sphere inside a box receives a deterministic escape manifold") {
    const auto sphere = body(
        1, mgv::physics::Collider::sphere(mgv::physics::Length{1.0F}), {});
    const auto box = body(
        2,
        mgv::physics::Collider::box(mgv::physics::Dimensions{{1.0F, 2.0F, 3.0F}}),
        {});

    const auto contact = mgv::physics::DiscreteCollisionDetector{}.detect(sphere, box);

    REQUIRE(contact.has_value());
    CHECK(contact->normal() == mgv::Vec3{-1.0F, 0.0F, 0.0F});
    CHECK(contact->penetration().metres() == Catch::Approx(2.0F));
}

TEST_CASE("discrete detector samples sphere-heightmap contacts in either order") {
    const auto sphere = body(
        1,
        mgv::physics::Collider::sphere(mgv::physics::Length{0.5F}),
        {0.5F, 0.25F, 0.5F});
    const auto terrain = body(
        2,
        mgv::physics::Collider::heightmap(
            2, 2, 1.0F, 1.0F, {0.0F, 0.0F, 0.0F, 0.0F}),
        {});
    const mgv::physics::DiscreteCollisionDetector detector;

    const auto sphere_first = detector.detect(sphere, terrain);
    const auto terrain_first = detector.detect(terrain, sphere);

    REQUIRE(sphere_first.has_value());
    REQUIRE(terrain_first.has_value());
    CHECK(sphere_first->normal() == mgv::Vec3{0.0F, -1.0F, 0.0F});
    CHECK(terrain_first->normal() == mgv::Vec3{0.0F, 1.0F, 0.0F});
    CHECK(sphere_first->penetration().metres() == Catch::Approx(0.25F));
    CHECK(terrain_first->penetration().metres() == Catch::Approx(0.25F));
}

TEST_CASE("a ball against a standing capsule is pushed out sideways") {
    // A tree trunk: 0.18 m through, standing from the ground to 4.8 m.
    const mgv::physics::DiscreteCollisionDetector detector;
    const auto trunk = body(
        1,
        mgv::physics::Collider::capsule(
            mgv::physics::Length{0.18F}, mgv::physics::Length{2.4F}),
        {0.0F, 2.4F, 0.0F});
    const auto ball = body(
        2, mgv::physics::Collider::sphere(mgv::physics::Length{0.021335F}),
        {0.19F, 1.0F, 0.0F});

    const auto contact = detector.detect(ball, trunk);

    REQUIRE(contact);
    // Horizontal, wherever up the trunk the ball strikes, because the closest
    // point on an upright axis is at the ball's own height.
    CHECK(contact->normal().x == Catch::Approx(-1.0F));
    CHECK(contact->normal().y == Catch::Approx(0.0F));
    CHECK(contact->penetration().metres() == Catch::Approx(0.011335F).margin(1.0e-5F));
}

TEST_CASE("a ball that misses the trunk does not touch it") {
    const mgv::physics::DiscreteCollisionDetector detector;
    const auto trunk = body(
        1,
        mgv::physics::Collider::capsule(
            mgv::physics::Length{0.18F}, mgv::physics::Length{2.4F}),
        {0.0F, 2.4F, 0.0F});

    // Past it on the side...
    CHECK_FALSE(detector.detect(
        body(2, mgv::physics::Collider::sphere(mgv::physics::Length{0.021335F}),
             {0.25F, 1.0F, 0.0F}),
        trunk));
    // ...and clean over the top of it, which is the segment clamp doing its
    // job rather than an infinite cylinder catching a ball in the sky.
    CHECK_FALSE(detector.detect(
        body(2, mgv::physics::Collider::sphere(mgv::physics::Length{0.021335F}),
             {0.0F, 9.0F, 0.0F}),
        trunk));
}

TEST_CASE("the capsule contact normal follows the order of the pair") {
    const mgv::physics::DiscreteCollisionDetector detector;
    const auto trunk_collider = mgv::physics::Collider::capsule(
        mgv::physics::Length{0.18F}, mgv::physics::Length{2.4F});
    const auto ball_collider =
        mgv::physics::Collider::sphere(mgv::physics::Length{0.021335F});

    const auto ball_first = detector.detect(
        body(1, ball_collider, {0.19F, 1.0F, 0.0F}),
        body(2, trunk_collider, {0.0F, 2.4F, 0.0F}));
    const auto trunk_first = detector.detect(
        body(1, trunk_collider, {0.0F, 2.4F, 0.0F}),
        body(2, ball_collider, {0.19F, 1.0F, 0.0F}));

    REQUIRE(ball_first);
    REQUIRE(trunk_first);
    // The normal always runs from the first body to the second.
    CHECK(ball_first->normal().x == Catch::Approx(-1.0F));
    CHECK(trunk_first->normal().x == Catch::Approx(1.0F));
    CHECK(ball_first->penetration().metres() ==
          Catch::Approx(trunk_first->penetration().metres()));
}
