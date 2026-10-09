#pragma once

#include "mgv/physics/units.hpp"

namespace mgv::network {

struct WeatherCondition {
    float temperature_c{};
    float wind_speed_mps{};
    float wind_direction_deg{};
    bool is_raining{};
    float turf_wetness{};
    /// Station pressure, in pascals. The engine's weather command has always
    /// taken one; nothing filled it in, so every course played at sea level
    /// whatever its elevation.
    float pressure_pascals{physics::standard_sea_level_pressure};
    /// Relative humidity, 0 to 1. Humid air is *less* dense than dry air at
    /// the same temperature, which is the opposite of what most people expect
    /// and worth a few percent of carry.
    float relative_humidity{};
};

} // namespace mgv::network
