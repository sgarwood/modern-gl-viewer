#pragma once

#include "mgv/network/datagram_transport.hpp"

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <variant>

namespace mgv::network {

struct DatagramReceived final {
    Datagram datagram;
};

struct NetworkFailure final {
    std::string message;
};

using NetworkEvent = std::variant<DatagramReceived, NetworkFailure>;

struct NetworkServiceConfiguration final {
    std::chrono::milliseconds poll_interval{10};
    std::size_t outgoing_capacity{1'024};
    std::size_t event_capacity{1'024};
};

struct NetworkStatistics final {
    std::uint64_t sent{};
    std::uint64_t received{};
    std::uint64_t dropped_events{};
    std::uint64_t transport_failures{};
};

class NetworkServiceImpl;

class NetworkService final {
public:
    explicit NetworkService(
        std::unique_ptr<DatagramTransport> transport,
        NetworkServiceConfiguration configuration = {});
    ~NetworkService();

    NetworkService(NetworkService&&) noexcept;
    NetworkService& operator=(NetworkService&&) noexcept;
    NetworkService(const NetworkService&) = delete;
    NetworkService& operator=(const NetworkService&) = delete;

    [[nodiscard]] bool try_send(Datagram datagram);
    [[nodiscard]] std::optional<NetworkEvent> poll_event();
    [[nodiscard]] std::optional<NetworkEvent> wait_for_event(
        std::chrono::milliseconds timeout);
    [[nodiscard]] bool running() const noexcept;
    [[nodiscard]] NetworkStatistics statistics() const noexcept;
    void stop() noexcept;

private:
    std::unique_ptr<NetworkServiceImpl> impl_;
};

} // namespace mgv::network
