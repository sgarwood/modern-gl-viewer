#include "mgv/camera_controller.hpp"

#include <algorithm>
#include <cmath>
#include <memory>
#include <numbers>
#include <utility>

namespace mgv {
namespace {

constexpr float orbit_step = 5.0F * std::numbers::pi_v<float> / 180.0F;
constexpr float maximum_pitch = 89.0F * std::numbers::pi_v<float> / 180.0F;
constexpr float zoom_in_factor = 0.9F;
constexpr float zoom_out_factor = 1.1F;
constexpr float minimum_distance = 0.05F;
constexpr float maximum_distance = 10'000.0F;

[[nodiscard]] Vec3 subtract(const Vec3& lhs, const Vec3& rhs) {
    return {lhs.x - rhs.x, lhs.y - rhs.y, lhs.z - rhs.z};
}

[[nodiscard]] float length(const Vec3& value) {
    return std::sqrt(value.x * value.x + value.y * value.y + value.z * value.z);
}

class OrbitCameraController final : public InputSink {
public:
    explicit OrbitCameraController(CameraTarget& target)
        : target_{target}, home_{target.camera()} {}

    void handle(InputAction action) override {
        if (action == InputAction::reset_view) {
            target_.set_camera(home_);
            return;
        }

        auto camera = target_.camera();
        const auto target = camera.target();
        const auto offset = subtract(camera.position(), target);
        const auto distance = length(offset);

        if (action == InputAction::zoom_in || action == InputAction::zoom_out) {
            const auto factor = action == InputAction::zoom_in ? zoom_in_factor : zoom_out_factor;
            const auto next_distance = std::clamp(distance * factor, minimum_distance, maximum_distance);
            camera.look_at({
                target.x + offset.x * next_distance / distance,
                target.y + offset.y * next_distance / distance,
                target.z + offset.z * next_distance / distance,
            }, target);
            target_.set_camera(std::move(camera));
            return;
        }

        auto yaw = std::atan2(offset.x, offset.z);
        auto pitch = std::asin(std::clamp(offset.y / distance, -1.0F, 1.0F));
        switch (action) {
        case InputAction::orbit_left:
            yaw -= orbit_step;
            break;
        case InputAction::orbit_right:
            yaw += orbit_step;
            break;
        case InputAction::orbit_up:
            pitch = std::min(pitch + orbit_step, maximum_pitch);
            break;
        case InputAction::orbit_down:
            pitch = std::max(pitch - orbit_step, -maximum_pitch);
            break;
        case InputAction::zoom_in:
        case InputAction::zoom_out:
        case InputAction::reset_view:
        case InputAction::fire_test_shot:
        case InputAction::toggle_range_finder:
        case InputAction::range_finder_ping:
            break;
        }

        const auto horizontal_distance = distance * std::cos(pitch);
        camera.look_at({
            target.x + horizontal_distance * std::sin(yaw),
            target.y + distance * std::sin(pitch),
            target.z + horizontal_distance * std::cos(yaw),
        }, target);
        target_.set_camera(std::move(camera));
    }

private:
    CameraTarget& target_;
    Camera home_;
};

} // namespace

std::unique_ptr<InputSink> make_orbit_camera_controller(CameraTarget& target) {
    return std::make_unique<OrbitCameraController>(target);
}

} // namespace mgv
