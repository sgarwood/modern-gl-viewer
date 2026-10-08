#include "mgv/compass.hpp"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <cmath>

TEST_CASE("a bearing of zero points along -Z, and the angle runs clockwise") {
    const auto north = mgv::bearing_to_direction(0.0F);
    CHECK(north.x == Catch::Approx(0.0F).margin(1.0e-6F));
    CHECK(north.z == Catch::Approx(-1.0F));

    const auto east = mgv::bearing_to_direction(90.0F);
    CHECK(east.x == Catch::Approx(1.0F));
    CHECK(east.z == Catch::Approx(0.0F).margin(1.0e-6F));

    const auto south = mgv::bearing_to_direction(180.0F);
    CHECK(south.z == Catch::Approx(1.0F));
}

TEST_CASE("every bearing comes back as a horizontal unit vector") {
    for (const auto degrees : {-180.0F, -37.0F, 0.0F, 12.5F, 90.0F, 271.0F, 359.9F}) {
        const auto direction = mgv::bearing_to_direction(degrees);
        CHECK(direction.y == 0.0F);
        CHECK(std::sqrt(direction.x * direction.x + direction.z * direction.z) ==
              Catch::Approx(1.0F));
    }
}

TEST_CASE("a bearing survives the round trip through a direction") {
    // Compared as headings rather than as numbers: a bearing means the same
    // thing modulo a full turn, and plus and minus a half turn are the same
    // way round. Asserting on the number instead fails at that seam while
    // the direction is exactly right.
    for (const auto degrees : {-180.0F, -179.0F, -90.0F, -1.0F, 0.0F, 45.0F, 123.0F, 180.0F}) {
        const auto direction = mgv::bearing_to_direction(degrees);
        const auto round_tripped = mgv::bearing_to_direction(
            mgv::direction_to_bearing(direction));
        CHECK(round_tripped.x == Catch::Approx(direction.x).margin(1.0e-5F));
        CHECK(round_tripped.z == Catch::Approx(direction.z).margin(1.0e-5F));
    }
}

TEST_CASE("the bearing of a vertical direction is not a crash") {
    CHECK(mgv::direction_to_bearing({0.0F, 1.0F, 0.0F}) == Catch::Approx(0.0F));
}
