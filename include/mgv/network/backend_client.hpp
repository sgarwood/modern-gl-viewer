#pragma once

#include "mgv/network/site_conditions.hpp"
#include "mgv/network/weather_condition.hpp"

#include <optional>
#include <string>
#include <vector>

namespace mgv::network {

// Easy abstraction for backend fetching so we can easily swap to Protobuf/gRPC later
class IBackendClient {
public:
    virtual ~IBackendClient() = default;

    // Fetch the weather for a specific course synchronously
    // (The EnvironmentSystem will call this on a background thread)
    virtual std::optional<WeatherCondition> fetch_course_weather(int course_id) = 0;

    /// Golf clubs matching a name or a place, nearest match first.
    ///
    /// Empty for no matches and empty when the search failed, which are the
    /// same thing to a player staring at a list. A caller that needs to tell
    /// them apart wants a different signature than this one.
    virtual std::vector<Club> search_clubs(const std::string& query) = 0;

    /// The elevation, weather and sun times over a point on the earth.
    ///
    /// Nothing when the service did not answer, and deliberately not a
    /// guess: the caller falls back to Greenwich at midday, which it can see
    /// it has done. Plausible numbers from a dead service it could not.
    virtual std::optional<SiteConditions> fetch_conditions(
        double latitude,
        double longitude,
        const std::string& name) = 0;
};

} // namespace mgv::network
