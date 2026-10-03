#include "mgv/network/datagram.hpp"

#include <stdexcept>
#include <utility>

namespace mgv::network {

Endpoint::Endpoint(std::string host, std::uint16_t port)
    : host_{std::move(host)}, port_{port} {
    if (host_.empty()) {
        throw std::invalid_argument{"Network endpoint host must not be empty"};
    }
    if (port_ == 0) {
        throw std::invalid_argument{"Network endpoint port must be positive"};
    }
}

const std::string& Endpoint::host() const noexcept { return host_; }
std::uint16_t Endpoint::port() const noexcept { return port_; }

Datagram::Datagram(Endpoint peer, std::vector<std::byte> payload)
    : peer_{std::move(peer)}, payload_{std::move(payload)} {
    if (payload_.size() > maximum_payload_size) {
        throw std::length_error{"UDP datagram payload exceeds 65507 bytes"};
    }
}

const Endpoint& Datagram::peer() const noexcept { return peer_; }
const std::vector<std::byte>& Datagram::payload() const noexcept { return payload_; }

} // namespace mgv::network
