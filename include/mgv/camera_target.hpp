#pragma once

#include "mgv/camera.hpp"

namespace mgv {

class CameraTarget {
public:
    virtual ~CameraTarget() = default;

    CameraTarget(const CameraTarget&) = delete;
    CameraTarget& operator=(const CameraTarget&) = delete;

    [[nodiscard]] virtual Camera camera() const = 0;
    virtual void set_camera(Camera camera) = 0;

protected:
    CameraTarget() = default;
    CameraTarget(CameraTarget&&) noexcept = default;
    CameraTarget& operator=(CameraTarget&&) noexcept = default;
};

} // namespace mgv
