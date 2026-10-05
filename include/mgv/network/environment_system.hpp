#pragma once

#include "mgv/network/backend_client.hpp"
#include <atomic>
#include <chrono>
#include <functional>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>

namespace mgv::network {

class EnvironmentSystem {
public:
    using WeatherCallback = std::function<void(const WeatherCondition&)>;

    explicit EnvironmentSystem(std::unique_ptr<IBackendClient> client);
    ~EnvironmentSystem();

    // Start the periodic polling thread
    void start(int course_id, std::chrono::seconds poll_interval = std::chrono::seconds(60));
    void stop();

    // Register a callback to be fired when new weather data arrives
    void on_weather_updated(WeatherCallback callback);

private:
    void poll_loop(int course_id, std::chrono::seconds poll_interval);

    std::unique_ptr<IBackendClient> client_;
    std::vector<WeatherCallback> callbacks_;
    std::mutex callback_mutex_;

    std::atomic<bool> is_running_{false};
    std::thread poll_thread_;
};

} // namespace mgv::network
