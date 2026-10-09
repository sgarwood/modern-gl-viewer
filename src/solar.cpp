#include "mgv/solar.hpp"

#include <algorithm>
#include <array>
#include <cmath>

namespace mgv {
namespace {

constexpr float pi = 3.14159265358979F;
constexpr float degrees_to_radians = pi / 180.0F;
constexpr float radians_to_degrees = 180.0F / pi;

/// The standard zenith for sunrise and sunset: the geometric horizon, plus
/// the refraction of the atmosphere and half the sun's own width.
constexpr float sunrise_zenith_degrees = 90.833F;

/// How far round its orbit the earth is, in radians, counting from the start
/// of the year. The quantity both the declination and the equation of time
/// are fitted against.
[[nodiscard]] float fractional_year(int day_of_year, float utc_hours) {
    const auto day = static_cast<float>(day_of_year) - 1.0F;
    return (2.0F * pi / 365.0F) * (day + ((utc_hours - 12.0F) / 24.0F));
}

/// How far ahead of or behind the clock the sun runs, in minutes. The earth's
/// orbit is an ellipse and its axis is tilted, so a solar day is not quite a
/// clock day, and the two drift apart by up to a quarter of an hour over a
/// year.
[[nodiscard]] float equation_of_time(float gamma) {
    return 229.18F * (0.000075F + (0.001868F * std::cos(gamma)) -
                      (0.032077F * std::sin(gamma)) -
                      (0.014615F * std::cos(2.0F * gamma)) -
                      (0.040849F * std::sin(2.0F * gamma)));
}

/// How far north or south of the equator the sun stands, in radians. Zero at
/// the equinoxes and 23.44 degrees either way at the solstices.
[[nodiscard]] float declination(float gamma) {
    return 0.006918F - (0.399912F * std::cos(gamma)) +
           (0.070257F * std::sin(gamma)) - (0.006758F * std::cos(2.0F * gamma)) +
           (0.000907F * std::sin(2.0F * gamma)) -
           (0.002697F * std::cos(3.0F * gamma)) +
           (0.001480F * std::sin(3.0F * gamma));
}

} // namespace

SunPosition sun_position(
    float latitude_degrees,
    float longitude_degrees,
    int day_of_year,
    float utc_hours) noexcept {
    const auto gamma = fractional_year(day_of_year, utc_hours);
    const auto declination_radians = declination(gamma);
    const auto latitude = latitude_degrees * degrees_to_radians;

    // True solar time, in minutes: the clock, corrected for the sun running
    // fast or slow, and for how far east or west of the meridian we stand --
    // four minutes per degree, which is the earth turning.
    const auto true_solar_minutes = (utc_hours * 60.0F) +
        equation_of_time(gamma) + (4.0F * longitude_degrees);
    // Noon is the sun on the meridian, so the hour angle is zero there.
    const auto hour_angle = ((true_solar_minutes / 4.0F) - 180.0F) * degrees_to_radians;

    const auto sine_altitude =
        (std::sin(latitude) * std::sin(declination_radians)) +
        (std::cos(latitude) * std::cos(declination_radians) * std::cos(hour_angle));
    const auto altitude = std::asin(std::clamp(sine_altitude, -1.0F, 1.0F));

    // Azimuth from north, clockwise. Taken through atan2 rather than an
    // arccosine so that it is continuous across noon and needs no separate
    // test for which side of the meridian the sun is on.
    const auto azimuth = std::atan2(
        -std::sin(hour_angle) * std::cos(declination_radians),
        (std::sin(declination_radians) * std::cos(latitude)) -
            (std::cos(declination_radians) * std::sin(latitude) * std::cos(hour_angle)));

    auto azimuth_degrees = azimuth * radians_to_degrees;
    if (azimuth_degrees < 0.0F) {
        azimuth_degrees += 360.0F;
    }
    return {
        .altitude_degrees = altitude * radians_to_degrees,
        .azimuth_degrees = azimuth_degrees,
    };
}

Daylight daylight(
    float latitude_degrees,
    float longitude_degrees,
    int day_of_year) noexcept {
    // Evaluated at noon, which is where the fit is centred for the day.
    const auto gamma = fractional_year(day_of_year, 12.0F);
    const auto declination_radians = declination(gamma);
    const auto latitude = latitude_degrees * degrees_to_radians;

    const auto cosine_hour_angle =
        (std::cos(sunrise_zenith_degrees * degrees_to_radians) /
         (std::cos(latitude) * std::cos(declination_radians))) -
        (std::tan(latitude) * std::tan(declination_radians));
    if (cosine_hour_angle > 1.0F || cosine_hour_angle < -1.0F) {
        // Polar night, or midnight sun: there is no hour at which the sun
        // crosses the horizon, and a time returned here would be a lie.
        return {.sunrise_hours = 0.0F, .sunset_hours = 0.0F, .rises = false};
    }

    const auto hour_angle = std::acos(cosine_hour_angle) * radians_to_degrees;
    const auto noon_minutes = 720.0F - (4.0F * longitude_degrees) - equation_of_time(gamma);
    return {
        .sunrise_hours = (noon_minutes - (4.0F * hour_angle)) / 60.0F,
        .sunset_hours = (noon_minutes + (4.0F * hour_angle)) / 60.0F,
        .rises = true,
    };
}

int day_of_year(int year, int month, int day) noexcept {
    constexpr std::array<int, 12> cumulative{0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334};
    if (month < 1 || month > 12) {
        return 1;
    }
    const auto leap = (year % 4 == 0 && year % 100 != 0) || year % 400 == 0;
    const auto leap_day = (leap && month > 2) ? 1 : 0;
    return cumulative[static_cast<std::size_t>(month - 1)] + day + leap_day;
}

} // namespace mgv
