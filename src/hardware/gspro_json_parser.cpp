#include "mgv/hardware/gspro_json_parser.hpp"
#include "mgv/hardware/json.hpp"

namespace mgv::hardware {

std::optional<ShotData> GSProJsonParser::parse(const std::string& payload) const {
    try {
        auto j = nlohmann::json::parse(payload);
        
        if (j.contains("BallData") && j["BallData"].is_object()) {
            const auto& ball_data = j["BallData"];
            
            float speed_mph = ball_data.value("Speed", 0.0f);
            float hla = ball_data.value("HLA", 0.0f);
            float vla = ball_data.value("VLA", 0.0f);
            float total_spin = ball_data.value("TotalSpin", 0.0f);
            float spin_axis = ball_data.value("SpinAxis", 0.0f);

            FullSwingData data{};
            data.ball_speed_mps = speed_mph * 0.44704f; // MPH to m/s
            data.launch_angle_deg = vla;
            data.launch_direction_deg = hla;
            data.total_spin_rpm = total_spin;
            data.spin_axis_deg = spin_axis;
            return ShotData{data};
        }
    } catch (const nlohmann::json::parse_error& /*e*/) {
        // Silently drop malformed payloads
    }
    
    return std::nullopt;
}

} // namespace mgv::hardware
