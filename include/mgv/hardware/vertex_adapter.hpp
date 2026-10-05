#pragma once

#include "mgv/hardware/launch_monitor.hpp"
#include "mgv/hardware/vertex_ble_scanner.hpp"
#include <memory>
#include <vector>
#include <cstdint>

namespace mgv::hardware {

class VertexAdapter final : public LaunchMonitor {
public:
    explicit VertexAdapter(std::unique_ptr<IVertexBleScanner> scanner);
    ~VertexAdapter() override { stop(); }
    
    void set_callback(ShotCallback callback) override;
    void start() override;
    void stop() override;

    // Test/Debug hook
    void simulate_putt_received(float speed, float launch, float direction, float spin, float axis);

private:
    void handle_gatt_payload(const std::vector<uint8_t>& payload);

    std::unique_ptr<IVertexBleScanner> scanner_;
    ShotCallback callback_;
    bool is_running_{false};
};

} // namespace mgv::hardware
