#include "mgv/hardware/mlm2_pro_adapter.hpp"
#include "mgv/hardware/vertex_adapter.hpp"
#include "mgv/hardware/shot_data.hpp"

#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

#include <optional>

using namespace mgv::hardware;

TEST_CASE("MLM2PRO adapter can trigger a simulated shot callback") {
    Mlm2ProAdapter adapter;
    std::optional<ShotData> received_shot;

    adapter.set_callback([&](const ShotData& data) {
        received_shot = data;
    });

    adapter.start();
    
    // Simulate receiving a network packet
    adapter.simulate_shot_received(160.0F, 12.0F, 1.5F, 2500.0F, -5.0F);
    
    adapter.stop();

    REQUIRE(received_shot.has_value());
    CHECK(received_shot->ball_speed_mps == Catch::Approx(160.0F));
    CHECK(received_shot->launch_angle_deg == Catch::Approx(12.0F));
    CHECK(received_shot->is_putt == false);
}

TEST_CASE("Vertex adapter can trigger a simulated putt callback") {
    VertexAdapter adapter;
    std::optional<ShotData> received_shot;

    adapter.set_callback([&](const ShotData& data) {
        received_shot = data;
    });

    adapter.start();
    
    // Simulate receiving Bluetooth data
    adapter.simulate_putt_received(10.0F, 0.0F, 0.5F, 50.0F, 0.0F);
    
    adapter.stop();

    REQUIRE(received_shot.has_value());
    CHECK(received_shot->ball_speed_mps == Catch::Approx(10.0F));
    CHECK(received_shot->is_putt == true);
}
