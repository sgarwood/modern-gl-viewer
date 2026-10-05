#pragma once

#include "mgv/hardware/launch_monitor.hpp"
#include <string>
#include <thread>

namespace mgv::hardware {

class Mlm2ProAdapter final : public LaunchMonitor {
public:
    ~Mlm2ProAdapter() override { stop(); }
    
    void set_callback(ShotCallback callback) override;
    void start() override;
    void stop() override;

    // Test/Debug hook
    void simulate_shot_received(float speed, float launch, float direction, float spin, float axis);

private:
    void parse_json_payload(const std::string& payload);

    ShotCallback callback_;
    bool is_running_{false};
    std::jthread listener_thread_;
};

} // namespace mgv::hardware
