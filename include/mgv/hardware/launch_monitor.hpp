#pragma once

#include "mgv/hardware/shot_data.hpp"
#include <functional>

namespace mgv::hardware {

class LaunchMonitor {
public:
    virtual ~LaunchMonitor() = default;

    using ShotCallback = std::function<void(const ShotData&)>;

    virtual void set_callback(ShotCallback callback) = 0;
    virtual void start() = 0;
    virtual void stop() = 0;
};

} // namespace mgv::hardware
