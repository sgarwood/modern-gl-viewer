#include "mgv/course_session.hpp"
#include "mgv/course_site.hpp"
#include "mgv/physics/units.hpp"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <filesystem>

TEST_CASE("an unconfigured course is played at Greenwich at midday") {
    // The fallback every build without a backend plays from: the GLFW shell,
    // the capture tool, and the Qt launcher before a club has been chosen.
    const mgv::CourseSite site;

    CHECK(site.name == "Greenwich");
    CHECK(site.latitude_degrees == Catch::Approx(51.4779F));
    CHECK(site.longitude_degrees == Catch::Approx(-0.0015F).margin(0.01F));
    CHECK(site.elevation_metres == 0.0F);
    CHECK(site.utc_hours == 12.0F);

    // Sea level, so the standard atmosphere and nothing taken off it.
    CHECK(mgv::physics::pressure_at_elevation(site.elevation_metres) ==
          Catch::Approx(mgv::physics::standard_sea_level_pressure));

    // Midday on the equinox, so the sun stands at ninety degrees less the
    // latitude, due south.
    const auto sun = mgv::sun_at(site);
    CHECK(sun.altitude_degrees == Catch::Approx(38.52F).margin(0.2F));
    CHECK(sun.azimuth_degrees == Catch::Approx(180.0F).margin(3.0F));
}

TEST_CASE("the bundled course session uses that fallback") {
    const std::filesystem::path assets{MGV_TEST_ASSETS};
    const auto description = mgv::default_course_session(assets);

    CHECK(description.site == mgv::CourseSite{});
}

TEST_CASE("a site points the environment's sun") {
    mgv::Environment environment;
    const mgv::CourseSite site;

    mgv::apply_site(environment, site);

    // Up, and to the south, which in this engine's compass is +Z.
    CHECK(environment.sun.direction.y == Catch::Approx(0.6224F).margin(0.01F));
    CHECK(environment.sun.direction.z > 0.7F);
    CHECK(environment.sun.direction.x == Catch::Approx(0.0F).margin(0.06F));
}

TEST_CASE("the season and the hour move the sun, not the course author") {
    // St Andrews, which is as far north as championship golf gets in Britain.
    const auto saint_andrews = [](int month, float hour) {
        return mgv::sun_at({
            .name = "St Andrews",
            .latitude_degrees = 56.3433F,
            .longitude_degrees = -2.8030F,
            .elevation_metres = 15.0F,
            .day_of_year = mgv::day_of_year(2026, month, 15),
            .utc_hours = hour,
        });
    };

    const auto june = saint_andrews(6, 12.0F);
    const auto december = saint_andrews(12, 12.0F);

    // Forty-seven degrees of declination between the solstices, all of which
    // a player sees in the length of their shadow.
    CHECK(june.altitude_degrees - december.altitude_degrees ==
          Catch::Approx(46.9F).margin(1.5F));
    CHECK(december.altitude_degrees < 12.0F);

    // And an evening sun is round to the west wherever it is in the year.
    CHECK(saint_andrews(6, 19.0F).azimuth_degrees > 270.0F);
}

TEST_CASE("elevation thins the air a ball flies through") {
    // Denver is a mile up. Eighteen percent less air is most of why a ball
    // carries further there.
    const auto sea_level = mgv::physics::pressure_at_elevation(0.0F);
    const auto denver = mgv::physics::pressure_at_elevation(1'609.0F);

    CHECK(denver == Catch::Approx(83'000.0F).margin(800.0F));
    CHECK(denver < sea_level);

    // Which is what the physics actually reads.
    CHECK(mgv::physics::air_density(15.0F, denver, 0.3F) <
          mgv::physics::air_density(15.0F, sea_level, 0.3F));
    // Gleneagles, a plausible British course at 200 m, loses under 3%.
    const auto gleneagles = mgv::physics::pressure_at_elevation(200.0F);
    CHECK(gleneagles / sea_level == Catch::Approx(0.977F).margin(0.005F));
}

TEST_CASE("Greenwich gets twelve hours of daylight at the equinox") {
    const auto times = mgv::daylight_at(mgv::CourseSite{});

    REQUIRE(times.rises);
    // A little over twelve, because sunrise and sunset are taken at the
    // moment the sun's upper edge clears a refracted horizon rather than at
    // its centre crossing a geometric one.
    CHECK(times.sunset_hours - times.sunrise_hours == Catch::Approx(12.15F).margin(0.15F));
}
