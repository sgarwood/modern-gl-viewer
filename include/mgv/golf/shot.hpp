#pragma once

#include <variant>

namespace mgv::golf {

struct FullSwingData final {
    float ball_speed_mps{};
    float launch_angle_deg{};
    float launch_direction_deg{};
    float total_spin_rpm{};
    float spin_axis_deg{};
};

struct PuttingData final {
    float putter_speed_mps{};
    float face_angle_deg{};
    float twist_deg{};
    float lie_angle_deg{};
    float shaft_lean_deg{};
    float loft_angle_deg{};
};

using Shot = std::variant<FullSwingData, PuttingData>;

} // namespace mgv::golf
