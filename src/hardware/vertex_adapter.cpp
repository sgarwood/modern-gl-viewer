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

void VertexAdapter::simulate_putt_received(float speed_mps, float face_deg, float twist_deg, float lie_deg, float lean_deg, float loft_deg) {
    if (is_running_ && callback_) {
        PuttingData data{};
        data.putter_speed_mps = speed_mps;
        data.face_angle_deg = face_deg;
        data.twist_deg = twist_deg;
        data.lie_angle_deg = lie_deg;
        data.shaft_lean_deg = lean_deg;
        data.loft_angle_deg = loft_deg;
        callback_(ShotData{data});
    }
}

void VertexAdapter::handle_gatt_payload(const std::vector<uint8_t>& payload) {
    if (!is_running_ || !callback_) return;
    
    // 12-byte payload mapped from Iottive metrics:
    // Byte 0-1: uint16 putter speed (mm/s)
    // Byte 2-3: int16 face angle (tenths of a degree)
    // Byte 4-5: int16 twist (tenths of a degree)
    // Byte 6-7: int16 lie angle (tenths of a degree)
    // Byte 8-9: int16 shaft lean (tenths of a degree)
    // Byte 10-11: int16 loft angle (tenths of a degree)
    if (payload.size() >= 12) {
        uint16_t speed_mms;
        int16_t face_angle;
        int16_t twist;
        int16_t lie_angle;
        int16_t shaft_lean;
        int16_t loft_angle;
        
        std::memcpy(&speed_mms, payload.data(), sizeof(speed_mms));
        std::memcpy(&face_angle, payload.data() + 2, sizeof(face_angle));
        std::memcpy(&twist, payload.data() + 4, sizeof(twist));
        std::memcpy(&lie_angle, payload.data() + 6, sizeof(lie_angle));
        std::memcpy(&shaft_lean, payload.data() + 8, sizeof(shaft_lean));
        std::memcpy(&loft_angle, payload.data() + 10, sizeof(loft_angle));

        PuttingData data{};
        data.putter_speed_mps = static_cast<float>(speed_mms) / 1000.0f;
        data.face_angle_deg = static_cast<float>(face_angle) / 10.0f;
        data.twist_deg = static_cast<float>(twist) / 10.0f;
        data.lie_angle_deg = static_cast<float>(lie_angle) / 10.0f;
        data.shaft_lean_deg = static_cast<float>(shaft_lean) / 10.0f;
        data.loft_angle_deg = static_cast<float>(loft_angle) / 10.0f;

        callback_(ShotData{data});
    }
}

} // namespace mgv::hardware
