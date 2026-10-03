#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace mgv::network {

class Endpoint final {
public:
    Endpoint(std::string host, std::uint16_t port);

    [[nodiscard]] const std::string& host() const noexcept;
    [[nodiscard]] std::uint16_t port() const noexcept;

    friend bool operator==(const Endpoint&, const Endpoint&) = default;

private:
    std::string host_;
    std::uint16_t port_{};
};

class Datagram final {
public:
    static constexpr std::size_t maximum_payload_size{65'507};

    Datagram(Endpoint peer, std::vector<std::byte> payload);

    [[nodiscard]] const Endpoint& peer() const noexcept;
    [[nodiscard]] const std::vector<std::byte>& payload() const noexcept;

private:
    Endpoint peer_;
    std::vector<std::byte> payload_;
};

} // namespace mgv::network
