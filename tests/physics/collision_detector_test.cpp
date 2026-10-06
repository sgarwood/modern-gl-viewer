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
