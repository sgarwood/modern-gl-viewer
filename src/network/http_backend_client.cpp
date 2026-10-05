#include "mgv/network/http_backend_client.hpp"
#include "mgv/network/httplib.hpp"
#include "mgv/hardware/json.hpp" // Reusing the JSON header we already pulled in

#include <iostream>

namespace mgv::network {

HttpBackendClient::HttpBackendClient(std::string base_url) 
    : base_url_(std::move(base_url)) {}

std::optional<WeatherCondition> HttpBackendClient::fetch_course_weather(int course_id) {
    httplib::Client cli(base_url_);
    
    // Set a reasonable timeout so we don't hang the background thread forever
    cli.set_connection_timeout(5, 0); // 5 seconds
    cli.set_read_timeout(5, 0);
    
    std::string path = "/courses/" + std::to_string(course_id) + "/weather";
    
    if (auto res = cli.Get(path.c_str())) {
        if (res->status == 200) {
            try {
                auto json = nlohmann::json::parse(res->body);
                
                WeatherCondition condition{};
                condition.temperature_c = json.value("temperatureC", 20.0f);
                condition.wind_speed_mps = json.value("windSpeedMps", 0.0f);
                condition.wind_direction_deg = json.value("windDirectionDeg", 0.0f);
                condition.is_raining = json.value("isRaining", false);
                condition.turf_wetness = json.value("turfWetness", 0.0f);
                
                return condition;
            } catch (const nlohmann::json::parse_error& e) {
                std::cerr << "Failed to parse weather JSON: " << e.what() << '\n';
            }
        } else {
            std::cerr << "Failed to fetch weather. HTTP Status: " << res->status << '\n';
        }
    } else {
        std::cerr << "Failed to connect to backend: " << to_string(res.error()) << '\n';
    }
    
    return std::nullopt;
}

} // namespace mgv::network
