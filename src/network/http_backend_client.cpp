#include "mgv/network/http_backend_client.hpp"
#include "mgv/network/httplib.hpp"
#include "mgv/hardware/json.hpp" // Reusing the JSON header we already pulled in

#include <string>

namespace mgv::network {

HttpBackendClient::HttpBackendClient(std::string base_url) 
    : base_url_(std::move(base_url)) {}

std::optional<WeatherCondition> HttpBackendClient::fetch_course_weather(int course_id) {
    httplib::Client cli(base_url_);
    cli.set_connection_timeout(5, 0);
    cli.set_read_timeout(5, 0);

    const auto path = "/courses/" + std::to_string(course_id) + "/weather";
    if (auto res = cli.Get(path.c_str())) {
        if (res->status == 200) {
            try {
                const auto json = nlohmann::json::parse(res->body);
                return WeatherCondition{
                    .temperature_c = json.at("temperatureC").get<float>(),
                    .wind_speed_mps = json.at("windSpeedMps").get<float>(),
                    .wind_direction_deg = json.at("windDirectionDeg").get<float>(),
                    .is_raining = json.at("isRaining").get<bool>(),
                    .turf_wetness = json.at("turfWetness").get<float>(),
                };
            } catch (const nlohmann::json::exception&) {
                return std::nullopt;
            }
        }
    }
    return std::nullopt;
}

} // namespace mgv::network
