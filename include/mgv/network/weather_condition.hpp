#pragma once

namespace mgv::network {

struct WeatherCondition {
    float temperature_c{};
    float wind_speed_mps{};
    float wind_direction_deg{};
    bool is_raining{};
    float turf_wetness{};
};

} // namespace mgv::network
