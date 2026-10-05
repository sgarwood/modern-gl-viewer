#include "mgv/hardware/mlm2_pro_adapter.hpp"
#include "mgv/hardware/vertex_adapter.hpp"
#include "mgv/hardware/gspro_json_parser.hpp"
#include "mgv/hardware/shot_data.hpp"

#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

#include <optional>

using namespace mgv::hardware;

TEST_CASE("MLM2PRO adapter can trigger a simulated shot callback") {
    Mlm2ProAdapter adapter{std::make_unique<GSProJsonParser>()};
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

class MockVertexBleScanner : public IVertexBleScanner {
public:
    void set_payload_callback(PayloadCallback callback) override {
        callback_ = callback;
    }
    void start() override {}
    void stop() override {}

    void simulate_gatt_notification(const std::vector<uint8_t>& bytes) {
        if (callback_) callback_(bytes);
    }
private:
    PayloadCallback callback_;
};

TEST_CASE("Vertex adapter parses GATT BLE payload into putt ShotData") {
    auto mock_scanner = std::make_unique<MockVertexBleScanner>();
    auto* scanner_ptr = mock_scanner.get();
    
    VertexAdapter adapter{std::move(mock_scanner)};
    std::optional<ShotData> received_shot;

    adapter.set_callback([&](const ShotData& data) {
        received_shot = data;
    });

    adapter.start();
    
    // Create a mock 8-byte payload matching our struct hypothesis
    // Speed: 2500 mm/s (0x09C4) -> little endian: 0xC4, 0x09
    // Face Angle: -15 tenths of a degree (-1.5 deg) (0xFFF1) -> little endian: 0xF1, 0xFF
    // Club Path: 10 tenths of a degree (0x000A) -> 0x0A, 0x00
    // Attack Angle: 0 (0x0000) -> 0x00, 0x00
    std::vector<uint8_t> payload = {0xC4, 0x09, 0xF1, 0xFF, 0x0A, 0x00, 0x00, 0x00};
    
    scanner_ptr->simulate_gatt_notification(payload);
    
    adapter.stop();

    REQUIRE(received_shot.has_value());
    CHECK(received_shot->ball_speed_mps == Catch::Approx(2.5F)); // 2500 mm/s = 2.5 m/s
    CHECK(received_shot->launch_direction_deg == Catch::Approx(-1.5F)); // -15 tenths
    CHECK(received_shot->is_putt == true);
    CHECK(received_shot->launch_angle_deg == Catch::Approx(0.0F)); // Putts are mostly flat
}

#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>
#include <thread>
#include <chrono>

TEST_CASE("MLM2PRO adapter receives and parses GSPro JSON over local TCP socket", "[integration]") {
    Mlm2ProAdapter adapter{std::make_unique<GSProJsonParser>()};
    std::optional<ShotData> received_shot;

    adapter.set_callback([&](const ShotData& data) {
        received_shot = data;
    });

    // 1. Start the server
    adapter.start();
    
    // Give the background thread a moment to bind the socket
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    // 2. Open a client socket to 127.0.0.1:921
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    REQUIRE(sock >= 0);

    struct sockaddr_in serv_addr{};
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(921);
    inet_pton(AF_INET, "127.0.0.1", &serv_addr.sin_addr);

    int connected = connect(sock, (struct sockaddr*)&serv_addr, sizeof(serv_addr));
    REQUIRE(connected == 0);

    // 3. Send realistic GSPro JSON payload
    std::string payload = R"({"BallData":{"Speed":160.0,"VLA":12.5,"HLA":-1.0,"TotalSpin":2500.0,"SpinAxis":2.0}})";
    send(sock, payload.c_str(), payload.size(), 0);
    close(sock);

    // 4. Give the server thread a moment to recv() and parse
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    adapter.stop();

    // 5. Assert the vertical slice successfully extracted and converted the data
    REQUIRE(received_shot.has_value());
    CHECK(received_shot->ball_speed_mps == Catch::Approx(160.0f * 0.44704f)); // MPH to m/s
    CHECK(received_shot->launch_angle_deg == Catch::Approx(12.5f));
    CHECK(received_shot->launch_direction_deg == Catch::Approx(-1.0f));
    CHECK(received_shot->total_spin_rpm == Catch::Approx(2500.0f));
    CHECK(received_shot->spin_axis_deg == Catch::Approx(2.0f));
    CHECK(received_shot->is_putt == false);
}
