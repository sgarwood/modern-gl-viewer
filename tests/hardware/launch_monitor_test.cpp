#include "mgv/hardware/mlm2_pro_adapter.hpp"
#include "mgv/hardware/vertex_adapter.hpp"
#include "mgv/hardware/gspro_json_parser.hpp"
#include "mgv/hardware/shot_data.hpp"

#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

#include <optional>
#include <variant>
#include <thread>

using namespace mgv::hardware;

TEST_CASE("MLM2PRO adapter can trigger a simulated shot callback") {
    Mlm2ProAdapter adapter{std::make_unique<GSProJsonParser>()};
    std::optional<ShotData> received_shot;

    adapter.set_callback([&](const ShotData& data) {
        received_shot = data;
    });

    adapter.start();
    
    // Simulate receiving a shot from the MLM2PRO
    adapter.simulate_shot_received(75.0F, 12.5F, 1.2F, 2500.0F, -2.5F);
    
    adapter.stop();

    REQUIRE(received_shot.has_value());
    REQUIRE(std::holds_alternative<FullSwingData>(received_shot.value()));
    
    auto fs_data = std::get<FullSwingData>(received_shot.value());
    CHECK(fs_data.ball_speed_mps == Catch::Approx(75.0F));
    CHECK(fs_data.launch_angle_deg == Catch::Approx(12.5F));
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
    
    // Create a mock 12-byte payload matching our struct hypothesis
    // Speed: 2500 mm/s (0x09C4) -> 0xC4, 0x09
    // Face Angle: -15 tenths (-1.5 deg) (0xFFF1) -> 0xF1, 0xFF
    // Twist: 5 tenths (0.5 deg) (0x0005) -> 0x05, 0x00
    // Lie: 700 tenths (70.0 deg) (0x02BC) -> 0xBC, 0x02
    // Lean: 20 tenths (2.0 deg) (0x0014) -> 0x14, 0x00
    // Loft: 30 tenths (3.0 deg) (0x001E) -> 0x1E, 0x00
    std::vector<uint8_t> payload = {
        0xC4, 0x09, 
        0xF1, 0xFF, 
        0x05, 0x00, 
        0xBC, 0x02, 
        0x14, 0x00, 
        0x1E, 0x00
    };
    
    scanner_ptr->simulate_gatt_notification(payload);
    
    adapter.stop();

    REQUIRE(received_shot.has_value());
    REQUIRE(std::holds_alternative<PuttingData>(received_shot.value()));
    
    auto putt_data = std::get<PuttingData>(received_shot.value());
    CHECK(putt_data.putter_speed_mps == Catch::Approx(2.5F)); // 2500 mm/s = 2.5 m/s
    CHECK(putt_data.face_angle_deg == Catch::Approx(-1.5F)); 
    CHECK(putt_data.twist_deg == Catch::Approx(0.5F)); 
    CHECK(putt_data.lie_angle_deg == Catch::Approx(70.0F)); 
    CHECK(putt_data.shaft_lean_deg == Catch::Approx(2.0F)); 
    CHECK(putt_data.loft_angle_deg == Catch::Approx(3.0F)); 
}

#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>
#include <chrono>

TEST_CASE("MLM2PRO adapter receives and parses GSPro JSON over local TCP socket", "[integration]") {
    Mlm2ProAdapter adapter{std::make_unique<GSProJsonParser>()};
    std::optional<ShotData> received_shot;

    adapter.set_callback([&](const ShotData& data) {
        received_shot = data;
    });

    adapter.start();

    // Give the server thread a moment to bind and listen
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    // Client socket to spoof GSPro payload
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    REQUIRE(sock != -1);

    struct sockaddr_in serv_addr{};
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(921);
    inet_pton(AF_INET, "127.0.0.1", &serv_addr.sin_addr);

    if (connect(sock, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) == 0) {
        std::string payload = R"({"BallData":{"Speed":150.0,"HLA":2.5,"VLA":10.5,"TotalSpin":2800.0,"SpinAxis":-5.0}})";
        send(sock, payload.c_str(), payload.size(), 0);
    }

    close(sock);

    // Give the server thread a moment to receive and parse
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    adapter.stop();

    REQUIRE(received_shot.has_value());
    REQUIRE(std::holds_alternative<FullSwingData>(received_shot.value()));
    
    auto fs_data = std::get<FullSwingData>(received_shot.value());
    CHECK(fs_data.ball_speed_mps == Catch::Approx(150.0F * 0.44704F)); // mph to m/s
    CHECK(fs_data.launch_angle_deg == Catch::Approx(10.5F));
    CHECK(fs_data.launch_direction_deg == Catch::Approx(2.5F));
    CHECK(fs_data.total_spin_rpm == Catch::Approx(2800.0F));
}
