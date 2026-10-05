#include "mgv/hardware/mlm2_pro_adapter.hpp"

namespace mgv::hardware {

void Mlm2ProAdapter::set_callback(ShotCallback callback) {
    callback_ = std::move(callback);
}

void Mlm2ProAdapter::start() {
    is_running_ = true;
    // TODO: Spin up UDP listener / jthread for MLM2Pro
}

void Mlm2ProAdapter::stop() {
    is_running_ = false;
    // TODO: Stop network listener
}

void Mlm2ProAdapter::simulate_shot_received(float speed, float launch, float direction, float spin, float axis) {
    if (is_running_ && callback_) {
        ShotData data{};
        data.ball_speed_mps = speed;
        data.launch_angle_deg = launch;
        data.launch_direction_deg = direction;
        data.total_spin_rpm = spin;
        data.spin_axis_deg = axis;
        data.is_putt = false;
        callback_(data);
    }
}

} // namespace mgv::hardware
