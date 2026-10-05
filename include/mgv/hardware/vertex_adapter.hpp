#pragma once

#include "mgv/hardware/launch_monitor.hpp"
#include <thread>

namespace mgv::hardware {

class VertexAdapter final : public LaunchMonitor {
public:
    ~VertexAdapter() override { stop(); }
    
    void set_callback(ShotCallback callback) override;
    void start() override;
    void stop() override;

    // Test/Debug hook
    void simulate_putt_received(float speed, float launch, float direction, float spin, float axis);

private:
    ShotCallback callback_;
    bool is_running_{false};
    std::jthread listener_thread_;
};

} // namespace mgv::hardware
