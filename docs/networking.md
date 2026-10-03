# Networking domain

The optional `mgv::network` target is a renderer- and frontend-independent bounded context. It
provides a small UDP MVP while keeping the concurrency policy separate from socket mechanics:

- `Endpoint` and `Datagram` are validated, owning value objects.
- `DatagramTransport` is the dependency-inversion port for network I/O.
- `UdpTransport` is the IPv4 UDP adapter and owns its socket through Pimpl/RAII.
- `NetworkService` is the façade and sole owner of a `std::jthread` worker.

The main/render thread never calls a socket. It submits outbound datagrams with non-blocking
`try_send()` and consumes immutable `NetworkEvent` values with `poll_event()` or
`wait_for_event()`. The worker is the only execution context that invokes `send()` and
`receive_for()` on the transport. This single-owner rule avoids socket-level locking and keeps
callbacks from unexpectedly executing inside rendering or gameplay code.

Both cross-thread queues are bounded. `try_send()` returns `false` when the service has stopped or
outbound backpressure is reached. When a slow consumer fills the event queue, the oldest event is
dropped and `NetworkStatistics::dropped_events` is incremented. Transport exceptions are contained
at the thread boundary and published as `NetworkFailure` events.

Shutdown is deterministic and idempotent. `stop()` requests the worker's stop token and joins it;
the transport contract requires `receive_for()` to return within the supplied polling interval.
The default interval bounds normal shutdown latency to approximately ten milliseconds. The façade
also stops and joins automatically during destruction.

Example:

```cpp
auto udp = std::make_unique<mgv::network::UdpTransport>(
    mgv::network::UdpBindConfiguration{.address = "127.0.0.1"});
mgv::network::NetworkService network{std::move(udp)};

network.try_send(mgv::network::Datagram{
    mgv::network::Endpoint{"127.0.0.1", 9000},
    {std::byte{0x01}, std::byte{0x02}},
});

while (auto event = network.poll_event()) {
    if (const auto* received = std::get_if<mgv::network::DatagramReceived>(&*event)) {
        // Decode received->datagram on the main/game thread.
    }
}
```

This slice deliberately provides datagram transport rather than gameplay replication. It does not
yet add message serialization/versioning, reliability and ordering, fragmentation, encryption,
authentication, connection state, clock synchronization, or authoritative entity snapshots. UDP's
65,507-byte protocol limit is enforced, but applications should choose a substantially smaller
payload budget appropriate to their path MTU.
