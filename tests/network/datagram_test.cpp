#include "mgv/network/datagram.hpp"

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <cstddef>
#include <stdexcept>
#include <string>
#include <vector>

TEST_CASE("network endpoints enforce domain invariants") {
    const mgv::network::Endpoint endpoint{"127.0.0.1", 4242};

    CHECK(endpoint.host() == "127.0.0.1");
    CHECK(endpoint.port() == 4242);
    CHECK(endpoint == mgv::network::Endpoint{"127.0.0.1", 4242});
    CHECK_THROWS_AS((mgv::network::Endpoint{"", 4242}), std::invalid_argument);
    CHECK_THROWS_AS(
        (mgv::network::Endpoint{"127.0.0.1", 0}), std::invalid_argument);
}

TEST_CASE("datagrams own a bounded binary payload") {
    const mgv::network::Endpoint endpoint{"localhost", 4242};
    const std::vector payload{std::byte{0x01}, std::byte{0x7f}};
    const mgv::network::Datagram datagram{endpoint, payload};

    CHECK(datagram.peer() == endpoint);
    CHECK(std::ranges::equal(datagram.payload(), payload));
    CHECK_THROWS_AS(
        (mgv::network::Datagram{
            endpoint,
            std::vector<std::byte>(mgv::network::Datagram::maximum_payload_size + 1),
        }),
        std::length_error);
}
