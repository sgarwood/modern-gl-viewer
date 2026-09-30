#include "mgv/camera_controller.hpp"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <cmath>

namespace {

class FakeCameraTarget final : public mgv::CameraTarget {
public:
    [[nodiscard]] mgv::Camera camera() const override { return camera_; }
    void set_camera(mgv::Camera camera) override {
        camera_ = camera;
        ++updates;
    }

    mgv::Camera camera_;
    int updates{};
};

[[nodiscard]] float distance(const mgv::Camera& camera) {
    const auto position = camera.position();
    const auto target = camera.target();
    const auto x = position.x - target.x;
    const auto y = position.y - target.y;
    const auto z = position.z - target.z;
    return std::sqrt(x * x + y * y + z * z);
}

} // namespace

TEST_CASE("orbit controller depends only on the camera target port") {
    FakeCameraTarget target;
    auto input = mgv::make_orbit_camera_controller(target);

    input->handle(mgv::InputAction::orbit_left);

    CHECK(target.updates == 1);
    CHECK(target.camera().target() == mgv::Vec3{0.0F, 0.0F, 0.0F});
    CHECK(target.camera().position().x < 0.0F);
    CHECK(distance(target.camera()) == Catch::Approx(3.0F));
}

TEST_CASE("orbit controller zooms without changing the look target") {
    FakeCameraTarget target;
    auto input = mgv::make_orbit_camera_controller(target);
    const auto initial_distance = distance(target.camera());

    input->handle(mgv::InputAction::zoom_in);

    CHECK(distance(target.camera()) < initial_distance);
    CHECK(target.camera().target() == mgv::Vec3{});
}

TEST_CASE("orbit controller resets all prior camera input") {
    FakeCameraTarget target;
    auto input = mgv::make_orbit_camera_controller(target);
    input->handle(mgv::InputAction::orbit_right);
    input->handle(mgv::InputAction::orbit_up);
    input->handle(mgv::InputAction::zoom_out);

    input->handle(mgv::InputAction::reset_view);

    CHECK(target.camera().position() == mgv::Vec3{0.0F, 0.0F, 3.0F});
    CHECK(target.camera().target() == mgv::Vec3{});
    CHECK(target.updates == 4);
}

TEST_CASE("orbit controller keeps repeated vertical input away from the up-axis singularity") {
    FakeCameraTarget target;
    auto input = mgv::make_orbit_camera_controller(target);

    for (int count = 0; count < 100; ++count) {
        input->handle(mgv::InputAction::orbit_up);
    }

    const auto position = target.camera().position();
    CHECK(std::isfinite(position.x));
    CHECK(std::isfinite(position.y));
    CHECK(std::isfinite(position.z));
    CHECK(std::abs(position.y) < distance(target.camera()));
}
