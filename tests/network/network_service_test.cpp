#include "mgv/network/network_service.hpp"

#include <catch2/catch_test_macros.hpp>

#include <chrono>
#include <condition_variable>
#include <cstddef>
#include <deque>
#include <memory>
#include <mutex>
#include <optional>
#include <stdexcept>
#include <string>
#include <thread>
#include <variant>
#include <vector>

namespace {

using namespace std::chrono_literals;

[[nodiscard]] mgv::network::Datagram test_datagram();

struct TransportState final {
    std::mutex mutex;
    std::condition_variable condition;
    std::deque<mgv::network::Datagram> incoming;
    std::vector<mgv::network::Datagram> sent;
    std::thread::id send_thread;
    std::thread::id receive_thread;
};

struct BlockingSendState final {
    std::mutex mutex;
    std::condition_variable condition;
    bool send_entered{};
    bool release_send{};
};

struct BurstState final {
    std::mutex mutex;
    std::condition_variable condition;
    int delivered{};
};

class FakeTransport final : public mgv::network::DatagramTransport {
public:
    explicit FakeTransport(std::shared_ptr<TransportState> state)
        : state_{std::move(state)} {}

    void send(const mgv::network::Datagram& datagram) override {
        {
            const std::lock_guard lock{state_->mutex};
            state_->send_thread = std::this_thread::get_id();
            state_->sent.push_back(datagram);
        }
        state_->condition.notify_all();
    }

    [[nodiscard]] std::optional<mgv::network::Datagram> receive_for(
        std::chrono::milliseconds timeout) override {
        std::unique_lock lock{state_->mutex};
        state_->receive_thread = std::this_thread::get_id();
        state_->condition.wait_for(lock, timeout, [this] {
            return !state_->incoming.empty();
        });
        if (state_->incoming.empty()) {
            return std::nullopt;
        }
        auto datagram = std::move(state_->incoming.front());
        state_->incoming.pop_front();
        return datagram;
    }

private:
    std::shared_ptr<TransportState> state_;
};

class FailingTransport final : public mgv::network::DatagramTransport {
public:
    void send(const mgv::network::Datagram&) override {}

    [[nodiscard]] std::optional<mgv::network::Datagram> receive_for(
        std::chrono::milliseconds) override {
        throw std::runtime_error{"test transport failed"};
    }
};

class BlockingSendTransport final : public mgv::network::DatagramTransport {
public:
    explicit BlockingSendTransport(std::shared_ptr<BlockingSendState> state)
        : state_{std::move(state)} {}

    void send(const mgv::network::Datagram&) override {
        std::unique_lock lock{state_->mutex};
        state_->send_entered = true;
        state_->condition.notify_all();
        state_->condition.wait_for(lock, 1s, [this] {
            return state_->release_send;
        });
    }

    [[nodiscard]] std::optional<mgv::network::Datagram> receive_for(
        std::chrono::milliseconds timeout) override {
        std::this_thread::sleep_for(timeout);
        return std::nullopt;
    }

private:
    std::shared_ptr<BlockingSendState> state_;
};

class BurstTransport final : public mgv::network::DatagramTransport {
public:
    explicit BurstTransport(std::shared_ptr<BurstState> state)
        : state_{std::move(state)} {}

    void send(const mgv::network::Datagram&) override {}

    [[nodiscard]] std::optional<mgv::network::Datagram> receive_for(
        std::chrono::milliseconds timeout) override {
        {
            const std::lock_guard lock{state_->mutex};
            if (state_->delivered < 3) {
                ++state_->delivered;
                state_->condition.notify_all();
                return test_datagram();
            }
        }
        std::this_thread::sleep_for(timeout);
        return std::nullopt;
    }

private:
    std::shared_ptr<BurstState> state_;
};

[[nodiscard]] mgv::network::Datagram test_datagram() {
    return {
        mgv::network::Endpoint{"127.0.0.1", 4242},
        {std::byte{0x42}},
    };
}

} // namespace

TEST_CASE("network service confines transport IO to its worker thread") {
    const auto caller_thread = std::this_thread::get_id();
    const auto state = std::make_shared<TransportState>();
    {
        const std::lock_guard lock{state->mutex};
        state->incoming.push_back(test_datagram());
    }
    mgv::network::NetworkServiceConfiguration configuration;
    configuration.poll_interval = 5ms;
    mgv::network::NetworkService service{
        std::make_unique<FakeTransport>(state), configuration};

    REQUIRE(service.try_send(test_datagram()));
    const auto event = service.wait_for_event(1s);

    REQUIRE(event.has_value());
    REQUIRE(std::holds_alternative<mgv::network::DatagramReceived>(*event));
    CHECK(std::get<mgv::network::DatagramReceived>(*event).datagram.payload() ==
          test_datagram().payload());
    {
        std::unique_lock lock{state->mutex};
        REQUIRE(state->condition.wait_for(lock, 1s, [&state] {
            return !state->sent.empty();
        }));
        CHECK(state->send_thread != caller_thread);
        CHECK(state->receive_thread != caller_thread);
        CHECK(state->send_thread == state->receive_thread);
    }
    service.stop();
    service.stop();
    CHECK_FALSE(service.running());
    CHECK_FALSE(service.try_send(test_datagram()));
    CHECK(service.statistics().sent == 1);
    CHECK(service.statistics().received == 1);
}

TEST_CASE("network service converts worker exceptions into failure events") {
    mgv::network::NetworkService service{std::make_unique<FailingTransport>()};

    const auto event = service.wait_for_event(1s);

    REQUIRE(event.has_value());
    REQUIRE(std::holds_alternative<mgv::network::NetworkFailure>(*event));
    CHECK(std::get<mgv::network::NetworkFailure>(*event).message ==
          "test transport failed");
    service.stop();
    CHECK_FALSE(service.running());
    CHECK_FALSE(service.try_send(test_datagram()));
    CHECK(service.statistics().transport_failures == 1);
}

TEST_CASE("network service validates bounded queue configuration") {
    mgv::network::NetworkServiceConfiguration configuration;
    configuration.outgoing_capacity = 0;

    CHECK_THROWS_AS(
        (mgv::network::NetworkService{
            std::make_unique<FailingTransport>(), configuration}),
        std::invalid_argument);
    CHECK_THROWS_AS(
        (mgv::network::NetworkService{
            std::unique_ptr<mgv::network::DatagramTransport>{}}),
        std::invalid_argument);
}

TEST_CASE("network service reports outbound backpressure without blocking callers") {
    const auto state = std::make_shared<BlockingSendState>();
    mgv::network::NetworkServiceConfiguration configuration;
    configuration.poll_interval = 5ms;
    configuration.outgoing_capacity = 1;
    mgv::network::NetworkService service{
        std::make_unique<BlockingSendTransport>(state), configuration};

    REQUIRE(service.try_send(test_datagram()));
    bool send_entered{};
    {
        std::unique_lock lock{state->mutex};
        send_entered = state->condition.wait_for(lock, 1s, [&state] {
            return state->send_entered;
        });
    }
    CHECK(send_entered);
    if (send_entered) {
        CHECK(service.try_send(test_datagram()));
        CHECK_FALSE(service.try_send(test_datagram()));
    }
    {
        const std::lock_guard lock{state->mutex};
        state->release_send = true;
    }
    state->condition.notify_all();
    service.stop();
}

TEST_CASE("network service bounds slow-consumer events and reports dropped data") {
    const auto state = std::make_shared<BurstState>();
    mgv::network::NetworkServiceConfiguration configuration;
    configuration.poll_interval = 5ms;
    configuration.event_capacity = 1;
    mgv::network::NetworkService service{
        std::make_unique<BurstTransport>(state), configuration};

    bool delivered{};
    {
        std::unique_lock lock{state->mutex};
        delivered = state->condition.wait_for(lock, 1s, [&state] {
            return state->delivered == 3;
        });
    }
    CHECK(delivered);
    service.stop();

    CHECK(service.statistics().received == 3);
    CHECK(service.statistics().dropped_events == 2);
    CHECK(service.poll_event().has_value());
    CHECK_FALSE(service.poll_event().has_value());
}
