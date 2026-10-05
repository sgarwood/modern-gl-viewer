#pragma once

#include "mgv/hardware/launch_monitor.hpp"

namespace mgv::hardware {

class Mlm2ProAdapter final : public LaunchMonitor {
public:
    void set_callback(ShotCallback callback) override;
    void start() override;
    void stop() override;

    // Test/Debug hook
    void simulate_shot_received(float speed, float launch, float direction, float spin, float axis);

private:
    ShotCallback callback_;
    bool is_running_{false};
};

} // namespace mgv::hardware
