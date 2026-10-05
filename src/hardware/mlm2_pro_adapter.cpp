#include "mgv/hardware/mlm2_pro_adapter.hpp"
#include <iostream>
#include <string>
#include <thread>
#include <vector>

#ifdef _WIN32
// Windows socket headers if we ever support it here
#else
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#endif

namespace mgv::hardware {

void Mlm2ProAdapter::set_callback(ShotCallback callback) {
    callback_ = std::move(callback);
}

void Mlm2ProAdapter::start() {
    if (is_running_) return;
    is_running_ = true;
    
    listener_thread_ = std::jthread([this](std::stop_token stoken) {
#ifndef _WIN32
        int server_fd = socket(AF_INET, SOCK_STREAM, 0);
        if (server_fd == -1) return;

        int opt = 1;
        setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt));

        struct sockaddr_in address{};
        address.sin_family = AF_INET;
        address.sin_addr.s_addr = INADDR_ANY;
        address.sin_port = htons(921); // GSPro default port

        if (bind(server_fd, (struct sockaddr*)&address, sizeof(address)) < 0) {
            close(server_fd);
            return;
        }

        if (listen(server_fd, 3) < 0) {
            close(server_fd);
            return;
        }

        // Use a timeout so we can periodically check the stop token
        struct timeval tv;
        tv.tv_sec = 1;
        tv.tv_usec = 0;
        setsockopt(server_fd, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv, sizeof tv);

        while (!stoken.stop_requested()) {
            int addrlen = sizeof(address);
            int client_socket = accept(server_fd, (struct sockaddr*)&address, (socklen_t*)&addrlen);
            
            if (client_socket >= 0) {
                char buffer[2048] = {0};
                // In a real robust server, we'd loop recv. For GSPro JSON, usually it's one small packet.
                ssize_t valread = recv(client_socket, buffer, sizeof(buffer) - 1, 0);
                if (valread > 0) {
                    std::string payload(buffer, valread);
                    parse_json_payload(payload);
                }
                close(client_socket);
            }
        }
        close(server_fd);
#endif
    });
}

void Mlm2ProAdapter::stop() {
    is_running_ = false;
    if (listener_thread_.joinable()) {
        listener_thread_.request_stop();
        listener_thread_.join();
    }
}

void Mlm2ProAdapter::simulate_shot_received(float speed, float launch, float direction, float spin, float axis) {
    if (is_running_ && callback_) {
        ShotData data{};
        data.ball_speed_mps = speed;
        data.launch_angle_deg = launch;
        data.launch_direction_deg = direction;
        data.total_spin_rpm = spin;
        data.spin_axis_deg = axis;
        data.is_putt = false;
        callback_(data);
    }
}

void Mlm2ProAdapter::parse_json_payload(const std::string& payload) {
    // A crude fallback JSON parser specifically for the expected GSPro keys
    // In production, nlohmann/json or simdjson should be used.
    auto extract_float = [&](const std::string& key) -> float {
        size_t pos = payload.find(key);
        if (pos == std::string::npos) return 0.0f;
        pos = payload.find(':', pos);
        if (pos == std::string::npos) return 0.0f;
        try {
            return std::stof(payload.substr(pos + 1));
        } catch (...) {
            return 0.0f;
        }
    };

    if (payload.find("BallData") != std::string::npos) {
        float speed_mph = extract_float("\"Speed\"");
        float hla = extract_float("\"HLA\"");
        float vla = extract_float("\"VLA\"");
        float total_spin = extract_float("\"TotalSpin\"");
        float spin_axis = extract_float("\"SpinAxis\"");

        if (callback_ && is_running_) {
            ShotData data{};
            data.ball_speed_mps = speed_mph * 0.44704f; // MPH to m/s
            data.launch_angle_deg = vla;
            data.launch_direction_deg = hla;
            data.total_spin_rpm = total_spin;
            data.spin_axis_deg = spin_axis;
            data.is_putt = false;
            callback_(data);
        }
    }
}

} // namespace mgv::hardware
