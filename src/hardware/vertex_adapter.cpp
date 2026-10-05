#include "mgv/hardware/vertex_adapter.hpp"

namespace mgv::hardware {

void VertexAdapter::set_callback(ShotCallback callback) {
    callback_ = std::move(callback);
}

void VertexAdapter::start() {
    is_running_ = true;
    // TODO: Spin up Bluetooth LE listener
}

void VertexAdapter::stop() {
    is_running_ = false;
    // TODO: Stop BT listener
}

void VertexAdapter::simulate_putt_received(float speed, float launch, float direction, float spin, float axis) {
    if (is_running_ && callback_) {
        ShotData data{};
        data.ball_speed_mps = speed;
        data.launch_angle_deg = launch;
        data.launch_direction_deg = direction;
        data.total_spin_rpm = spin;
        data.spin_axis_deg = axis;
        data.is_putt = true;
        callback_(data);
    }
}

} // namespace mgv::hardware
