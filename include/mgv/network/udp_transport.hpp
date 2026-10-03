#pragma once

#include "mgv/network/datagram_transport.hpp"

#include <chrono>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>

namespace mgv::network {

struct UdpBindConfiguration final {
    std::string address{"127.0.0.1"};
    std::uint16_t port{};
};

class UdpTransportImpl;

class UdpTransport final : public DatagramTransport {
public:
    explicit UdpTransport(UdpBindConfiguration configuration = {});
    ~UdpTransport() override;

    UdpTransport(UdpTransport&&) noexcept;
    UdpTransport& operator=(UdpTransport&&) noexcept;

    [[nodiscard]] const Endpoint& local_endpoint() const noexcept;
    void send(const Datagram& datagram) override;
    [[nodiscard]] std::optional<Datagram> receive_for(
        std::chrono::milliseconds timeout) override;

private:
    std::unique_ptr<UdpTransportImpl> impl_;
};

} // namespace mgv::network
