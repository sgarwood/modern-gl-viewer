#include "mgv/network/udp_transport.hpp"

#include <array>
#include <cerrno>
#include <chrono>
#include <cstring>
#include <limits>
#include <stdexcept>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <arpa/inet.h>
#include <netdb.h>
#include <poll.h>
#include <sys/socket.h>
#include <unistd.h>
#endif

namespace mgv::network {
namespace {

#ifdef _WIN32
using NativeSocket = SOCKET;
using SocketLength = int;
constexpr NativeSocket invalid_socket{INVALID_SOCKET};

class WinsockRuntime final {
public:
    WinsockRuntime() {
        WSADATA data{};
        const auto result = WSAStartup(MAKEWORD(2, 2), &data);
        if (result != 0) {
            throw std::system_error{result, std::system_category(), "WSAStartup failed"};
        }
    }

    ~WinsockRuntime() { WSACleanup(); }

    WinsockRuntime(const WinsockRuntime&) = delete;
    WinsockRuntime& operator=(const WinsockRuntime&) = delete;
};

void ensure_socket_runtime() {
    static const WinsockRuntime runtime;
    static_cast<void>(runtime);
}

[[nodiscard]] int last_socket_error() { return WSAGetLastError(); }

void close_socket(NativeSocket socket) noexcept {
    if (socket != invalid_socket) {
        closesocket(socket);
    }
}

[[nodiscard]] int wait_readable(NativeSocket socket, std::chrono::milliseconds timeout) {
    WSAPOLLFD descriptor{};
    descriptor.fd = socket;
    descriptor.events = POLLRDNORM;
    return WSAPoll(&descriptor, 1, static_cast<INT>(timeout.count()));
}
#else
using NativeSocket = int;
using SocketLength = socklen_t;
constexpr NativeSocket invalid_socket{-1};

void ensure_socket_runtime() {}

[[nodiscard]] int last_socket_error() { return errno; }

void close_socket(NativeSocket socket) noexcept {
    if (socket != invalid_socket) {
        close(socket);
    }
}

[[nodiscard]] int wait_readable(NativeSocket socket, std::chrono::milliseconds timeout) {
    pollfd descriptor{};
    descriptor.fd = socket;
    descriptor.events = POLLIN;
    return poll(&descriptor, 1, static_cast<int>(timeout.count()));
}
#endif

[[noreturn]] void throw_socket_error(const char* operation) {
    throw std::system_error{last_socket_error(), std::system_category(), operation};
}

[[nodiscard]] sockaddr_in resolve_ipv4(const std::string& host, std::uint16_t port) {
    addrinfo hints{};
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_DGRAM;
    hints.ai_protocol = IPPROTO_UDP;
    addrinfo* addresses{};
    const auto service = std::to_string(port);
    const auto result = getaddrinfo(host.c_str(), service.c_str(), &hints, &addresses);
    if (result != 0 || addresses == nullptr) {
#ifdef _WIN32
        throw std::runtime_error{"Unable to resolve UDP endpoint: " +
                                 std::to_string(result)};
#else
        throw std::runtime_error{"Unable to resolve UDP endpoint: " +
                                 std::string{gai_strerror(result)}};
#endif
    }
    sockaddr_in address{};
    std::memcpy(&address, addresses->ai_addr, sizeof(address));
    freeaddrinfo(addresses);
    return address;
}

[[nodiscard]] std::string numeric_address(const sockaddr_in& address) {
    std::array<char, INET_ADDRSTRLEN> buffer{};
    if (inet_ntop(AF_INET, &address.sin_addr, buffer.data(), buffer.size()) == nullptr) {
        throw_socket_error("Unable to format UDP address");
    }
    return buffer.data();
}

} // namespace

class UdpTransportImpl final {
public:
    explicit UdpTransportImpl(const UdpBindConfiguration& configuration) {
        ensure_socket_runtime();
        if (configuration.address.empty()) {
            throw std::invalid_argument{"UDP bind address must not be empty"};
        }
        socket = ::socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
        if (socket == invalid_socket) {
            throw_socket_error("Unable to create UDP socket");
        }
        try {
            const auto requested = resolve_ipv4(configuration.address, configuration.port);
            if (::bind(
                    socket,
                    reinterpret_cast<const sockaddr*>(&requested),
                    static_cast<SocketLength>(sizeof(requested))) != 0) {
                throw_socket_error("Unable to bind UDP socket");
            }
            sockaddr_in bound{};
            SocketLength bound_size{sizeof(bound)};
            if (getsockname(
                    socket,
                    reinterpret_cast<sockaddr*>(&bound),
                    &bound_size) != 0) {
                throw_socket_error("Unable to inspect UDP socket");
            }
            local_endpoint = std::make_unique<Endpoint>(
                numeric_address(bound), ntohs(bound.sin_port));
        } catch (...) {
            close_socket(socket);
            socket = invalid_socket;
            throw;
        }
    }

    ~UdpTransportImpl() { close_socket(socket); }

    void send(const Datagram& datagram) const {
        const auto destination = resolve_ipv4(
            datagram.peer().host(), datagram.peer().port());
#ifdef _WIN32
        const auto payload_size = static_cast<int>(datagram.payload().size());
        const auto result = sendto(
            socket,
            reinterpret_cast<const char*>(datagram.payload().data()),
            payload_size,
            0,
            reinterpret_cast<const sockaddr*>(&destination),
            static_cast<int>(sizeof(destination)));
#else
        const auto result = sendto(
            socket,
            datagram.payload().data(),
            datagram.payload().size(),
            0,
            reinterpret_cast<const sockaddr*>(&destination),
            static_cast<SocketLength>(sizeof(destination)));
#endif
        if (result < 0) {
            throw_socket_error("Unable to send UDP datagram");
        }
        if (static_cast<std::size_t>(result) != datagram.payload().size()) {
            throw std::runtime_error{"UDP socket sent a partial datagram"};
        }
    }

    [[nodiscard]] std::optional<Datagram> receive_for(
        std::chrono::milliseconds timeout) const {
        if (timeout < std::chrono::milliseconds::zero()) {
            throw std::invalid_argument{"UDP receive timeout must not be negative"};
        }
        if (timeout.count() > std::numeric_limits<int>::max()) {
            throw std::invalid_argument{"UDP receive timeout is too large"};
        }
        const auto ready = wait_readable(socket, timeout);
        if (ready < 0) {
#ifndef _WIN32
            if (errno == EINTR) {
                return std::nullopt;
            }
#endif
            throw_socket_error("Unable to poll UDP socket");
        }
        if (ready == 0) {
            return std::nullopt;
        }

        std::array<std::byte, Datagram::maximum_payload_size> buffer{};
        sockaddr_in source{};
#ifdef _WIN32
        int source_size{sizeof(source)};
        const auto received_size = recvfrom(
            socket,
            reinterpret_cast<char*>(buffer.data()),
            static_cast<int>(buffer.size()),
            0,
            reinterpret_cast<sockaddr*>(&source),
            &source_size);
#else
        socklen_t source_size{sizeof(source)};
        const auto received_size = recvfrom(
            socket,
            buffer.data(),
            buffer.size(),
            0,
            reinterpret_cast<sockaddr*>(&source),
            &source_size);
#endif
        if (received_size < 0) {
            throw_socket_error("Unable to receive UDP datagram");
        }
        std::vector<std::byte> payload(
            buffer.begin(), buffer.begin() + received_size);
        return Datagram{
            Endpoint{numeric_address(source), ntohs(source.sin_port)},
            std::move(payload),
        };
    }

    NativeSocket socket{invalid_socket};
    std::unique_ptr<Endpoint> local_endpoint;
};

UdpTransport::UdpTransport(UdpBindConfiguration configuration)
    : impl_{std::make_unique<UdpTransportImpl>(configuration)} {}

UdpTransport::~UdpTransport() = default;
UdpTransport::UdpTransport(UdpTransport&&) noexcept = default;
UdpTransport& UdpTransport::operator=(UdpTransport&&) noexcept = default;

const Endpoint& UdpTransport::local_endpoint() const noexcept {
    return *impl_->local_endpoint;
}

void UdpTransport::send(const Datagram& datagram) { impl_->send(datagram); }

std::optional<Datagram> UdpTransport::receive_for(
    std::chrono::milliseconds timeout) {
    return impl_->receive_for(timeout);
}

} // namespace mgv::network
