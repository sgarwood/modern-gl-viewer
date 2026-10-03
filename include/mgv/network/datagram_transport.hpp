#pragma once

#include "mgv/network/datagram.hpp"

#include <chrono>
#include <optional>

namespace mgv::network {

class DatagramTransport {
public:
    virtual ~DatagramTransport() = default;
    DatagramTransport(const DatagramTransport&) = delete;
    DatagramTransport& operator=(const DatagramTransport&) = delete;

    virtual void send(const Datagram& datagram) = 0;
    [[nodiscard]] virtual std::optional<Datagram> receive_for(
        std::chrono::milliseconds timeout) = 0;

protected:
    DatagramTransport() = default;
    DatagramTransport(DatagramTransport&&) noexcept = default;
    DatagramTransport& operator=(DatagramTransport&&) noexcept = default;
};

} // namespace mgv::network
