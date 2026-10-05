#pragma once

#include "mgv/network/weather_condition.hpp"
#include <optional>

namespace mgv::network {

// Easy abstraction for backend fetching so we can easily swap to Protobuf/gRPC later
class IBackendClient {
public:
    virtual ~IBackendClient() = default;

    // Fetch the weather for a specific course synchronously
    // (The EnvironmentSystem will call this on a background thread)
    virtual std::optional<WeatherCondition> fetch_course_weather(int course_id) = 0;
};

} // namespace mgv::network
