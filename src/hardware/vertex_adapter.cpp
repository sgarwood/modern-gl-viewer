#include "mgv/hardware/vertex_adapter.hpp"
#include <cstring>

namespace mgv::hardware {

VertexAdapter::VertexAdapter(std::unique_ptr<IVertexBleScanner> scanner)
    : scanner_(std::move(scanner)) {
    if (scanner_) {
        scanner_->set_payload_callback([this](const std::vector<uint8_t>& payload) {
            handle_gatt_payload(payload);
        });
    }
}

void VertexAdapter::set_callback(ShotCallback callback) {
    callback_ = std::move(callback);
}

void VertexAdapter::start() {
    is_running_ = true;
    if (scanner_) {
        scanner_->start();
    }
}

void VertexAdapter::stop() {
    is_running_ = false;
    if (scanner_) {
        scanner_->stop();
    }
}

void VertexAdapter::simulate_putt_received(float speed, float launch, float direction, float spin, float axis) {
    if (is_running_ && callback_) {
        ShotData data{};
        data.ball_speed_mps = speed;
        data.launch_angle_deg = launch;
        data.launch_direction_deg = direction;
        data.total_spin_rpm = spin;
        data.spin_axis_deg = axis;
        data.is_putt = true;
        callback_(data);
    }
}

void VertexAdapter::handle_gatt_payload(const std::vector<uint8_t>& payload) {
    if (!is_running_ || !callback_) return;
    
    // We expect a minimum 8-byte payload representing the putting metrics struct
    // Byte 0-1: uint16 putter speed (mm/s)
    // Byte 2-3: int16 face angle (tenths of a degree)
    // Byte 4-5: int16 club path (tenths of a degree)
    // Byte 6-7: int16 attack angle (tenths of a degree)
    if (payload.size() >= 8) {
        uint16_t speed_mms;
        int16_t face_angle;
        int16_t club_path;
        
        std::memcpy(&speed_mms, payload.data(), sizeof(speed_mms));
        std::memcpy(&face_angle, payload.data() + 2, sizeof(face_angle));
        std::memcpy(&club_path, payload.data() + 4, sizeof(club_path));

        ShotData data{};
        // Convert mm/s to m/s. Assuming 1.0 smash factor for putter, ball speed = club speed.
        data.ball_speed_mps = static_cast<float>(speed_mms) / 1000.0f;
        
        // Putter face angle roughly dictates launch direction for putts (approx 90% influence)
        data.launch_direction_deg = static_cast<float>(face_angle) / 10.0f;
        
        // Putts launch along the ground mostly
        data.launch_angle_deg = 0.0f;
        data.total_spin_rpm = 50.0f; // Minimal forward roll
        data.spin_axis_deg = 0.0f;
        data.is_putt = true;

        callback_(data);
    }
}

} // namespace mgv::hardware
