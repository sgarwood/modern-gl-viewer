#pragma once

#include "mgv/hardware/launch_monitor.hpp"
#include "mgv/hardware/shot_data_parser.hpp"
#include <atomic>
#include <cstdint>
#include <string>
#include <thread>
#include <memory>

namespace mgv::hardware {

class Mlm2ProAdapter final : public LaunchMonitor {
public:
    explicit Mlm2ProAdapter(
        std::unique_ptr<IShotDataParser> parser,
        std::uint16_t port = 921);
    ~Mlm2ProAdapter() override { stop(); }
    
    void set_callback(ShotCallback callback) override;
    void start() override;
    void stop() override;

    // Test/Debug hook
    void simulate_shot_received(float speed, float launch, float direction, float spin, float axis);

private:
    void handle_payload(const std::string& payload);

    std::unique_ptr<IShotDataParser> parser_;
    ShotCallback callback_;
    std::uint16_t port_;
    std::atomic_bool is_running_{false};
    std::jthread listener_thread_;
};

} // namespace mgv::hardware
