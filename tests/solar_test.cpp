#include "mgv/solar.hpp"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

namespace {
// The Royal Observatory, which is also the fallback every build that cannot
// ask a backend where it is plays from.
constexpr float greenwich_latitude = 51.4779F;
constexpr float greenwich_longitude = -0.0015F;
} // namespace

TEST_CASE("the midday sun stands where the latitude and the season put it") {
    // At solar noon the sun's altitude is 90 degrees, less the latitude, plus
    // the declination -- which runs from zero at an equinox to 23.44 degrees
    // either way at a solstice. At Greenwich that is 38.5 degrees in March,
    // 62.0 in June and 15.1 in December, and those three numbers are in every
    // almanac ever printed.
    const auto equinox = mgv::sun_position(
        greenwich_latitude, greenwich_longitude, mgv::day_of_year(2026, 3, 21), 12.0F);
    const auto midsummer = mgv::sun_position(
        greenwich_latitude, greenwich_longitude, mgv::day_of_year(2026, 6, 21), 12.0F);
    const auto midwinter = mgv::sun_position(
        greenwich_latitude, greenwich_longitude, mgv::day_of_year(2026, 12, 21), 12.0F);

    CHECK(equinox.altitude_degrees == Catch::Approx(38.52F).margin(0.15F));
    CHECK(midsummer.altitude_degrees == Catch::Approx(61.96F).margin(0.15F));
    CHECK(midwinter.altitude_degrees == Catch::Approx(15.08F).margin(0.15F));

    // Due south at noon, give or take the equation of time -- the sun runs up
    // to a quarter of an hour ahead of or behind the clock, so clock noon is
    // not solar noon and the bearing is a degree or two off 180. That is the
    // correction doing its job, not an error in it.
    for (const auto& noon : {equinox, midsummer, midwinter}) {
        CHECK(noon.azimuth_degrees == Catch::Approx(180.0F).margin(3.0F));
    }
}

TEST_CASE("the sun rises in the east and sets in the west") {
    const auto day = mgv::day_of_year(2026, 6, 21);
    const auto morning = mgv::sun_position(greenwich_latitude, greenwich_longitude, day, 7.0F);
    const auto evening = mgv::sun_position(greenwich_latitude, greenwich_longitude, day, 18.0F);

    CHECK(morning.azimuth_degrees > 45.0F);
    CHECK(morning.azimuth_degrees < 135.0F);
    CHECK(evening.azimuth_degrees > 225.0F);
    CHECK(evening.azimuth_degrees < 315.0F);
    // Both well up on a midsummer evening, which is why a round can be played
    // at either end of the day in June and not in December.
    CHECK(morning.altitude_degrees > 0.0F);
    CHECK(evening.altitude_degrees > 0.0F);
}

TEST_CASE("the sun is below the horizon at night") {
    const auto midnight = mgv::sun_position(
        greenwich_latitude, greenwich_longitude, mgv::day_of_year(2026, 12, 21), 0.0F);

    // Reported rather than clamped: a caller needs to tell dusk from a sun a
    // degree above the horizon, and a clamp to zero makes those the same.
    CHECK(midnight.altitude_degrees < -40.0F);
}

TEST_CASE("sunrise and sunset agree with the weather service") {
    // Checked against Open-Meteo's own answer for St Andrews Major Golf Club,
    // 51.4257 N 3.2496 W, on 9 October 2026: it gave 06:26 and 17:32 UTC.
    // This computes 06:25 and 17:35. Five minutes is the tolerance because
    // the Fourier fit below is good to about a minute and the service's
    // figure carries the elevation of its own grid cell, which moves a sunset
    // later; agreeing to the second would mean one of us was rounding to the
    // other.
    const auto times = mgv::daylight(51.4257F, -3.2496F, mgv::day_of_year(2026, 10, 9));

    REQUIRE(times.rises);
    CHECK(times.sunrise_hours == Catch::Approx(6.0F + (26.0F / 60.0F)).margin(5.0F / 60.0F));
    CHECK(times.sunset_hours == Catch::Approx(17.0F + (32.0F / 60.0F)).margin(5.0F / 60.0F));
    // A little over eleven hours of golf in October.
    CHECK(times.sunset_hours - times.sunrise_hours == Catch::Approx(11.1F).margin(0.2F));
}

TEST_CASE("a polar night has no sunrise to report") {
    // Tromso in late December. Returning a time here would be a lie, and a
    // caller that lit a scene from it would get a sun that never came up
    // pretending to rise at some hour.
    const auto polar = mgv::daylight(69.65F, 18.96F, mgv::day_of_year(2026, 12, 21));

    CHECK_FALSE(polar.rises);
    // And the same place in June, where it is the other way about.
    CHECK_FALSE(mgv::daylight(69.65F, 18.96F, mgv::day_of_year(2026, 6, 21)).rises);
}

TEST_CASE("the day of the year counts the leap day") {
    CHECK(mgv::day_of_year(2026, 1, 1) == 1);
    CHECK(mgv::day_of_year(2026, 10, 9) == 282);
    CHECK(mgv::day_of_year(2026, 12, 31) == 365);
    // 2024 is a leap year, 2026 is not, so the first of March moves.
    CHECK(mgv::day_of_year(2024, 3, 1) == 61);
    CHECK(mgv::day_of_year(2026, 3, 1) == 60);
    // 1900 was not a leap year and 2000 was, which is the rule most
    // implementations get wrong.
    CHECK(mgv::day_of_year(1900, 3, 1) == 60);
    CHECK(mgv::day_of_year(2000, 3, 1) == 61);
}
