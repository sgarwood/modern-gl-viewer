#pragma once

namespace mgv::hardware {

struct ShotData {
    float ball_speed_mps{};
    float launch_angle_deg{};
    float launch_direction_deg{};
    float total_spin_rpm{};
    float spin_axis_deg{};
    bool is_putt{}; // true if the shot came from a putting sensor
};

} // namespace mgv::hardware
