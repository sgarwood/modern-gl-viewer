#pragma once

#include "mgv/camera_target.hpp"
#include "mgv/input.hpp"

#include <memory>

namespace mgv {

[[nodiscard]] std::unique_ptr<InputSink> make_orbit_camera_controller(CameraTarget& target);

} // namespace mgv
