#include "mgv/lighting.hpp"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <cmath>

namespace {

[[nodiscard]] float length(const mgv::Vec3& value) {
    return std::sqrt(value.x * value.x + value.y * value.y + value.z * value.z);
}

} // namespace

TEST_CASE("light directions are normalized") {
    const auto direction = mgv::normalized_light_direction({0.0F, 4.0F, 3.0F});

    CHECK(length(direction) == Catch::Approx(1.0F));
    CHECK(direction.y == Catch::Approx(0.8F));
    CHECK(direction.z == Catch::Approx(0.6F));
}

TEST_CASE("a degenerate light direction falls back to the default sun") {
    const auto direction = mgv::normalized_light_direction({0.0F, 0.0F, 0.0F});

    CHECK(length(direction) == Catch::Approx(1.0F));
    CHECK(direction.y > 0.0F);
}

TEST_CASE("a sun directly overhead points straight up") {
    const auto direction = mgv::sun_direction_from_angles(0.0F, 90.0F);

    CHECK(direction.x == Catch::Approx(0.0F).margin(1.0e-6F));
    CHECK(direction.y == Catch::Approx(1.0F));
    CHECK(direction.z == Catch::Approx(0.0F).margin(1.0e-6F));
}

TEST_CASE("sun azimuth is measured clockwise from north") {
    const auto north = mgv::sun_direction_from_angles(0.0F, 0.0F);
    CHECK(north.z == Catch::Approx(-1.0F));

    const auto east = mgv::sun_direction_from_angles(90.0F, 0.0F);
    CHECK(east.x == Catch::Approx(1.0F));
    CHECK(east.z == Catch::Approx(0.0F).margin(1.0e-6F));
}

TEST_CASE("a sun at altitude keeps a unit length") {
    const auto direction = mgv::sun_direction_from_angles(135.0F, 40.0F);

    CHECK(length(direction) == Catch::Approx(1.0F));
    CHECK(direction.y == Catch::Approx(std::sin(40.0F * 3.14159265F / 180.0F)).epsilon(1.0e-4F));
}
