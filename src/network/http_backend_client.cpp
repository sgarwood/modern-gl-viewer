#include "mgv/network/http_backend_client.hpp"
#include "mgv/network/httplib.hpp"
#include "mgv/hardware/json.hpp" // Reusing the JSON header we already pulled in

#include <sstream>
#include <string>

namespace mgv::network {
namespace {

/// Five seconds, because the launcher blocks a player on this and a search
/// that takes longer than that has failed whatever it eventually returns.
constexpr int timeout_seconds = 5;

[[nodiscard]] httplib::Client connect(const std::string& base_url) {
    httplib::Client client(base_url);
    client.set_connection_timeout(timeout_seconds, 0);
    client.set_read_timeout(timeout_seconds, 0);
    return client;
}

/// A number from a field that may be absent, which several of these are when
/// a provider has nothing to say about them.
[[nodiscard]] float number_or(const nlohmann::json& json, const char* key, float fallback) {
    const auto found = json.find(key);
    return found != json.end() && found->is_number() ? found->get<float>() : fallback;
}

[[nodiscard]] WeatherCondition weather_from(const nlohmann::json& json) {
    return WeatherCondition{
        .temperature_c = json.at("temperatureC").get<float>(),
        .wind_speed_mps = json.at("windSpeedMps").get<float>(),
        .wind_direction_deg = json.at("windDirectionDeg").get<float>(),
        .is_raining = json.at("isRaining").get<bool>(),
        .turf_wetness = json.at("turfWetness").get<float>(),
        // Added to the contract after the first clients shipped, so absence
        // means a backend that predates them rather than a malformed reply.
        .pressure_pascals =
            number_or(json, "pressurePascals", physics::standard_sea_level_pressure),
        .relative_humidity = number_or(json, "relativeHumidity", 0.0F),
    };
}

/// Formats a coordinate for a query string.
///
/// Through a stream with the classic locale rather than std::to_string,
/// because a machine running under a comma-decimal locale would otherwise
/// send `latitude=51,4257` and be told its request was malformed.
[[nodiscard]] std::string coordinate(double value) {
    std::ostringstream out;
    out.imbue(std::locale::classic());
    out << value;
    return out.str();
}

} // namespace

HttpBackendClient::HttpBackendClient(std::string base_url)
    : base_url_(std::move(base_url)) {}

std::optional<WeatherCondition> HttpBackendClient::fetch_course_weather(int course_id) {
    auto client = connect(base_url_);
    const auto path = "/courses/" + std::to_string(course_id) + "/weather";
    if (auto res = client.Get(path.c_str())) {
        if (res->status == 200) {
            try {
                return weather_from(nlohmann::json::parse(res->body));
            } catch (const nlohmann::json::exception&) {
                return std::nullopt;
            }
        }
    }
    return std::nullopt;
}

std::vector<Club> HttpBackendClient::search_clubs(const std::string& query) {
    auto client = connect(base_url_);
    httplib::Params params{{"query", query}};
    const auto path = "/clubs?" + httplib::detail::params_to_query_str(params);

    std::vector<Club> clubs;
    if (auto res = client.Get(path.c_str())) {
        if (res->status == 200) {
            try {
                const auto json = nlohmann::json::parse(res->body);
                for (const auto& entry : json) {
                    clubs.push_back(Club{
                        .id = entry.value("id", 0LL),
                        .name = entry.value("name", std::string{}),
                        .address = entry.value("address", std::string{}),
                        .latitude = entry.at("latitude").get<double>(),
                        .longitude = entry.at("longitude").get<double>(),
                    });
                }
            } catch (const nlohmann::json::exception&) {
                // A half-parsed list is worse than none: a player would pick
                // the third club and get the second one's coordinates.
                return {};
            }
        }
    }
    return clubs;
}

std::optional<SiteConditions> HttpBackendClient::fetch_conditions(
    double latitude,
    double longitude,
    const std::string& name) {
    auto client = connect(base_url_);
    httplib::Params params{
        {"latitude", coordinate(latitude)},
        {"longitude", coordinate(longitude)},
        {"name", name},
    };
    const auto path = "/conditions?" + httplib::detail::params_to_query_str(params);

    if (auto res = client.Get(path.c_str())) {
        if (res->status == 200) {
            try {
                const auto json = nlohmann::json::parse(res->body);
                return SiteConditions{
                    .name = json.value("name", name),
                    .latitude = json.at("latitude").get<double>(),
                    .longitude = json.at("longitude").get<double>(),
                    .elevation_metres = number_or(json, "elevationMetres", 0.0F),
                    .day_of_year = json.at("dayOfYear").get<int>(),
                    .utc_hours = json.at("utcHours").get<float>(),
                    .sunrise_utc_hours = number_or(json, "sunriseUtcHours", 0.0F),
                    .sunset_utc_hours = number_or(json, "sunsetUtcHours", 0.0F),
                    .weather = weather_from(json.at("weather")),
                };
            } catch (const nlohmann::json::exception&) {
                return std::nullopt;
            }
        }
    }
    return std::nullopt;
}

} // namespace mgv::network
