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

Mlm2ProAdapter::Mlm2ProAdapter(
    std::unique_ptr<IShotDataParser> parser,
    std::uint16_t port)
    : parser_(std::move(parser)), port_{port} {}

void Mlm2ProAdapter::set_callback(ShotCallback callback) {
    callback_ = std::move(callback);
}

void Mlm2ProAdapter::start() {
    if (is_running_.exchange(true)) return;
    
    listener_thread_ = std::jthread([this](std::stop_token stoken) {
#ifndef _WIN32
        int server_fd = socket(AF_INET, SOCK_STREAM, 0);
        if (server_fd == -1) return;

        int opt = 1;
        setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
#ifdef SO_REUSEPORT
        setsockopt(server_fd, SOL_SOCKET, SO_REUSEPORT, &opt, sizeof(opt));
#endif

        struct sockaddr_in address{};
        address.sin_family = AF_INET;
        address.sin_addr.s_addr = INADDR_ANY;
        address.sin_port = htons(port_);

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
                ssize_t valread = recv(client_socket, buffer, sizeof(buffer) - 1, 0);
                if (valread > 0) {
                    std::string payload(buffer, valread);
                    handle_payload(payload);
                }
                close(client_socket);
            }
        }
        close(server_fd);
#endif
    });
}

void Mlm2ProAdapter::stop() {
    is_running_.store(false);
    if (listener_thread_.joinable()) {
        listener_thread_.request_stop();
        listener_thread_.join();
    }
}

void Mlm2ProAdapter::simulate_shot_received(float speed, float launch, float direction, float spin, float axis) {
    if (is_running_.load() && callback_) {
        FullSwingData data{};
        data.ball_speed_mps = speed;
        data.launch_angle_deg = launch;
        data.launch_direction_deg = direction;
        data.total_spin_rpm = spin;
        data.spin_axis_deg = axis;
        callback_(ShotData{data});
    }
}

void Mlm2ProAdapter::handle_payload(const std::string& payload) {
    if (!parser_) return;
    
    auto parsed_data = parser_->parse(payload);
    if (parsed_data.has_value() && callback_ && is_running_.load()) {
        callback_(parsed_data.value());
    }
}

} // namespace mgv::hardware
