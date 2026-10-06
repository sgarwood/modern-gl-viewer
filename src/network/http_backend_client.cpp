#include "mgv/network/http_backend_client.hpp"
#include "mgv/network/httplib.hpp"
#include "mgv/hardware/json.hpp" // Reusing the JSON header we already pulled in

#include <iostream>

namespace mgv::network {

HttpBackendClient::HttpBackendClient(std::string base_url) 
    : base_url_(std::move(base_url)) {}

std::optional<WeatherCondition> HttpBackendClient::fetch_course_weather(int course_id) {
    // Hardcode Open-Meteo for St. Andrews Golf Course for the vertical slice!
    // (In full version, backend returns lat/lon for course_id)
    httplib::Client cli("http://api.open-meteo.com");
    cli.set_connection_timeout(5, 0);
    cli.set_read_timeout(5, 0);
    
    // St. Andrews coordinates
    std::string path = "/v1/forecast?latitude=56.34&longitude=-2.81&current_weather=true";
    
    if (auto res = cli.Get(path.c_str())) {
        if (res->status == 200) {
            try {
                auto json = nlohmann::json::parse(res->body);
                auto current = json["current_weather"];
                
                WeatherCondition condition{};
                condition.temperature_c = current.value("temperature", 20.0f);
                // Open-Meteo gives wind in km/h. Convert to m/s:
                float wind_kmh = current.value("windspeed", 0.0f);
                condition.wind_speed_mps = wind_kmh / 3.6f;
                condition.wind_direction_deg = current.value("winddirection", 0.0f);
                condition.is_raining = false; // We can parse weathercode later
                condition.turf_wetness = 0.0f;
                
                std::cout << "Fetched Live Weather for St. Andrews: " 
                          << condition.temperature_c << "C, Wind: " 
                          << condition.wind_speed_mps << " m/s\n";
                          
                return condition;
            } catch (const nlohmann::json::exception& e) {
                std::cerr << "Failed to parse weather JSON: " << e.what() << '\n';
            }
        }
    }
    return std::nullopt;
}

} // namespace mgv::network
