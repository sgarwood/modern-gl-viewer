#include "mgv/network/environment_system.hpp"

namespace mgv::network {

EnvironmentSystem::EnvironmentSystem(std::unique_ptr<IBackendClient> client)
    : client_(std::move(client)) {}

EnvironmentSystem::~EnvironmentSystem() {
    stop();
}

void EnvironmentSystem::start(int course_id, std::chrono::seconds poll_interval) {
    if (is_running_) return;
    is_running_ = true;
    poll_thread_ = std::thread(&EnvironmentSystem::poll_loop, this, course_id, poll_interval);
}

void EnvironmentSystem::stop() {
    is_running_ = false;
    if (poll_thread_.joinable()) {
        poll_thread_.join();
    }
}

void EnvironmentSystem::on_weather_updated(WeatherCallback callback) {
    std::lock_guard<std::mutex> lock(callback_mutex_);
    callbacks_.push_back(std::move(callback));
}

void EnvironmentSystem::poll_loop(int course_id, std::chrono::seconds poll_interval) {
    while (is_running_) {
        auto weather_opt = client_->fetch_course_weather(course_id);
        
        if (weather_opt) {
            std::lock_guard<std::mutex> lock(callback_mutex_);
            for (const auto& cb : callbacks_) {
                cb(*weather_opt);
            }
        }
        
        // Sleep in small increments to allow fast shutdown
        for (int i = 0; i < poll_interval.count() * 10 && is_running_; ++i) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    }
}

} // namespace mgv::network
