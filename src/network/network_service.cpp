#include "mgv/network/network_service.hpp"

#include <atomic>
#include <condition_variable>
#include <deque>
#include <exception>
#include <mutex>
#include <stdexcept>
#include <thread>
#include <utility>

namespace mgv::network {

class NetworkServiceImpl final {
public:
    NetworkServiceImpl(
        std::unique_ptr<DatagramTransport> value,
        NetworkServiceConfiguration service_configuration)
        : transport{std::move(value)}, configuration{service_configuration} {
        if (!transport) {
            throw std::invalid_argument{"Network service requires a transport"};
        }
        if (configuration.poll_interval <= std::chrono::milliseconds::zero()) {
            throw std::invalid_argument{"Network poll interval must be positive"};
        }
        if (configuration.outgoing_capacity == 0 || configuration.event_capacity == 0) {
            throw std::invalid_argument{"Network queue capacities must be positive"};
        }
        worker = std::jthread{[this](std::stop_token stop_token) {
            run(stop_token);
        }};
    }

    ~NetworkServiceImpl() { stop(); }

    [[nodiscard]] bool try_send(Datagram datagram) {
        const std::lock_guard lock{outgoing_mutex};
        if (!is_running.load() || outgoing.size() >= configuration.outgoing_capacity) {
            return false;
        }
        outgoing.push_back(std::move(datagram));
        return true;
    }

    [[nodiscard]] std::optional<NetworkEvent> poll_event() {
        const std::lock_guard lock{event_mutex};
        return take_event();
    }

    [[nodiscard]] std::optional<NetworkEvent> wait_for_event(
        std::chrono::milliseconds timeout) {
        if (timeout < std::chrono::milliseconds::zero()) {
            throw std::invalid_argument{"Network event timeout must not be negative"};
        }
        std::unique_lock lock{event_mutex};
        event_condition.wait_for(lock, timeout, [this] {
            return !events.empty() || !is_running.load();
        });
        return take_event();
    }

    void stop() noexcept {
        const std::lock_guard lifecycle_lock{lifecycle_mutex};
        if (worker.joinable()) {
            worker.request_stop();
            worker.join();
        }
        is_running.store(false);
        event_condition.notify_all();
    }

    [[nodiscard]] NetworkStatistics statistics() const noexcept {
        return {
            .sent = sent.load(),
            .received = received.load(),
            .dropped_events = dropped_events.load(),
            .transport_failures = transport_failures.load(),
        };
    }

    void run(std::stop_token stop_token) noexcept {
        try {
            while (!stop_token.stop_requested()) {
                drain_outgoing();
                if (stop_token.stop_requested()) {
                    break;
                }
                auto datagram = transport->receive_for(configuration.poll_interval);
                if (datagram) {
                    received.fetch_add(1);
                    publish(DatagramReceived{std::move(*datagram)});
                }
            }
        } catch (const std::exception& error) {
            transport_failures.fetch_add(1);
            publish(NetworkFailure{error.what()});
        } catch (...) {
            transport_failures.fetch_add(1);
            publish(NetworkFailure{"Unknown network transport failure"});
        }
        is_running.store(false);
        event_condition.notify_all();
    }

    void drain_outgoing() {
        std::deque<Datagram> pending;
        {
            const std::lock_guard lock{outgoing_mutex};
            pending.swap(outgoing);
        }
        for (const auto& datagram : pending) {
            transport->send(datagram);
            sent.fetch_add(1);
        }
    }

    void publish(NetworkEvent event) {
        {
            const std::lock_guard lock{event_mutex};
            if (events.size() == configuration.event_capacity) {
                events.pop_front();
                dropped_events.fetch_add(1);
            }
            events.push_back(std::move(event));
        }
        event_condition.notify_one();
    }

    [[nodiscard]] std::optional<NetworkEvent> take_event() {
        if (events.empty()) {
            return std::nullopt;
        }
        auto event = std::move(events.front());
        events.pop_front();
        return event;
    }

    std::unique_ptr<DatagramTransport> transport;
    NetworkServiceConfiguration configuration;
    mutable std::mutex outgoing_mutex;
    std::deque<Datagram> outgoing;
    mutable std::mutex event_mutex;
    std::condition_variable event_condition;
    std::deque<NetworkEvent> events;
    std::atomic_bool is_running{true};
    std::atomic<std::uint64_t> sent{};
    std::atomic<std::uint64_t> received{};
    std::atomic<std::uint64_t> dropped_events{};
    std::atomic<std::uint64_t> transport_failures{};
    mutable std::mutex lifecycle_mutex;
    std::jthread worker;
};

NetworkService::NetworkService(
    std::unique_ptr<DatagramTransport> transport,
    NetworkServiceConfiguration configuration)
    : impl_{std::make_unique<NetworkServiceImpl>(
          std::move(transport), configuration)} {}

NetworkService::~NetworkService() = default;
NetworkService::NetworkService(NetworkService&&) noexcept = default;
NetworkService& NetworkService::operator=(NetworkService&&) noexcept = default;

bool NetworkService::try_send(Datagram datagram) {
    return impl_->try_send(std::move(datagram));
}

std::optional<NetworkEvent> NetworkService::poll_event() {
    return impl_->poll_event();
}

std::optional<NetworkEvent> NetworkService::wait_for_event(
    std::chrono::milliseconds timeout) {
    return impl_->wait_for_event(timeout);
}

bool NetworkService::running() const noexcept { return impl_->is_running.load(); }

NetworkStatistics NetworkService::statistics() const noexcept {
    return impl_->statistics();
}

void NetworkService::stop() noexcept { impl_->stop(); }

} // namespace mgv::network
