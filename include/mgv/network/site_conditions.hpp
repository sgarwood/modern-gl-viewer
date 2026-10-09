#pragma once

#include "mgv/course_site.hpp"
#include "mgv/network/weather_condition.hpp"

#include <string>

namespace mgv::network {

/// A golf club the backend found, and where it is.
struct Club final {
    /// The directory provider's own identifier, so a client can ask again
    /// without repeating the search.
    long long id{};
    std::string name;
    /// The full address, which is how a player tells two clubs of the same
    /// name apart -- and there are a great many Royal somethings.
    std::string address;
    double latitude{};
    double longitude{};
};

/// Where and when a round is played, and the weather over it.
///
/// The wire form of everything the game needs before it can render a frame.
/// One object rather than three requests, because the elevation and the
/// weather come out of the same upstream call and the client needs all of it
/// at once.
struct SiteConditions final {
    std::string name;
    double latitude{};
    double longitude{};
    float elevation_metres{};
    int day_of_year{};
    float utc_hours{};
    float sunrise_utc_hours{};
    float sunset_utc_hours{};
    WeatherCondition weather{};
};

/// The site these conditions describe.
///
/// Inline, and a plain field copy, so that the wire form and the thing the
/// renderer and the solver read stay separate types. They look alike today;
/// the moment the backend adds a field the game does not care about, or the
/// game needs one no service reports, they stop.
[[nodiscard]] inline CourseSite course_site_of(const SiteConditions& conditions) {
    return CourseSite{
        .name = conditions.name,
        .latitude_degrees = static_cast<float>(conditions.latitude),
        .longitude_degrees = static_cast<float>(conditions.longitude),
        .elevation_metres = conditions.elevation_metres,
        .day_of_year = conditions.day_of_year,
        .utc_hours = conditions.utc_hours,
    };
}

} // namespace mgv::network
