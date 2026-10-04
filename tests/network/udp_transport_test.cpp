#include "mgv/network/network_service.hpp"
#include "mgv/network/udp_transport.hpp"

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <memory>
#include <variant>
#include <vector>

TEST_CASE("UDP services exchange a datagram over the loopback interface") {
    using namespace std::chrono_literals;

    auto receiving_transport = std::make_unique<mgv::network::UdpTransport>(
        mgv::network::UdpBindConfiguration{.address = "127.0.0.1"});
    const auto receiver = receiving_transport->local_endpoint();
    mgv::network::NetworkService receiving{std::move(receiving_transport)};
    mgv::network::NetworkService sending{
        std::make_unique<mgv::network::UdpTransport>(
            mgv::network::UdpBindConfiguration{.address = "127.0.0.1"})};
    const std::vector payload{std::byte{0xde}, std::byte{0xad}, std::byte{0xbe}, std::byte{0xef}};

    REQUIRE(sending.try_send(mgv::network::Datagram{receiver, payload}));
    const auto event = receiving.wait_for_event(2s);

    REQUIRE(event.has_value());
    REQUIRE(std::holds_alternative<mgv::network::DatagramReceived>(*event));
    const auto& received = std::get<mgv::network::DatagramReceived>(*event).datagram;
    CHECK(std::ranges::equal(received.payload(), payload));
    CHECK(received.peer().host() == "127.0.0.1");
}
