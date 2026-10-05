#pragma once

#include <vector>
#include <cstdint>
#include <functional>

namespace mgv::hardware {

class IVertexBleScanner {
public:
    virtual ~IVertexBleScanner() = default;

    using PayloadCallback = std::function<void(const std::vector<uint8_t>&)>;

    // Registers the callback to fire when a GATT notification payload is received
    virtual void set_payload_callback(PayloadCallback callback) = 0;
    
    // Starts the BLE scan and subscription process
    virtual void start() = 0;
    
    // Stops the BLE scan and disconnects
    virtual void stop() = 0;
};

} // namespace mgv::hardware
