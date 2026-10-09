#pragma once

namespace mgv {

/// Where the sun is, seen from a point on the earth at a moment.
struct SunPosition final {
    /// Degrees above the horizon. Negative when the sun has set, which is
    /// worth having rather than clamping: a caller needs to know the
    /// difference between dusk and a sun one degree up.
    float altitude_degrees{};
    /// Compass bearing of the sun, in the engine's convention: zero is north,
    /// measured clockwise, so a southern sun reads 180. The same convention
    /// `bearing_to_direction` and the shot model use, deliberately.
    float azimuth_degrees{};
};

/// When the sun rises and sets, in hours UTC.
struct Daylight final {
    float sunrise_hours{};
    float sunset_hours{};
    /// False inside the polar circles on the days the sun does not manage to
    /// rise or to set, when the two times above are meaningless.
    bool rises{true};
};

/// The sun's position for a place and a moment.
///
/// `longitude_degrees` is east-positive, so Britain is a little negative.
/// `day_of_year` is 1 on the first of January. `utc_hours` runs 0 to 24.
///
/// This is the NOAA solar position algorithm: a Fourier fit to the equation
/// of time and the declination, then spherical trigonometry for the rest. It
/// is good to a fraction of a degree, which is far inside anything a sky or a
/// shadow will show, and it needs no ephemeris table.
///
/// It exists because a round is played somewhere real at some real time, and
/// the sun was a direction typed into a description. A course in Scotland in
/// October has a low sun in the south and long shadows; the same course in
/// June has it high and nearly overhead at noon. Nothing about that can be
/// authored per hole.
[[nodiscard]] SunPosition sun_position(
    float latitude_degrees,
    float longitude_degrees,
    int day_of_year,
    float utc_hours) noexcept;

/// Sunrise and sunset for a place and a date, in hours UTC.
///
/// Taken at the standard zenith of 90.833 degrees, which is the geometric
/// horizon plus the refraction of the atmosphere and half the sun's own
/// width -- the convention every almanac and weather service uses, so that
/// these agree with the times one hands us.
[[nodiscard]] Daylight daylight(
    float latitude_degrees,
    float longitude_degrees,
    int day_of_year) noexcept;

/// The day of the year for a calendar date, 1 for the first of January.
[[nodiscard]] int day_of_year(int year, int month, int day) noexcept;

} // namespace mgv
