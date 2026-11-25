# Socket shim (UDP)

Socket-shaped code can run in the sim: `simulnetx::udp::socket` is a
minimal async Asio-style UDP socket. An address is a node plus a port
(node ids are addresses; there is no IP layer). One send is one sim
message, so loss, partitions, crashes, duplication and reorder apply
to datagrams exactly as they do to messages, and the trace shows them.

```cpp
#include "simulnetx/udp.hxx"

simulnetx::udp::socket server(net, server_node);
server.bind(8080);
std::array<std::byte, 64> buf{};
server.async_receive_from(buf,
    [&](std::error_code ec, std::size_t n, simulnetx::udp::endpoint from) {
        server.async_send_to(as_bytes("pong"), from, [](auto, auto) {});
    });

simulnetx::udp::socket client(net, client_node);
client.async_send_to(as_bytes("ping"), {server_node.id(), 8080},
                     [](auto, auto) {});
```

No completion handler ever runs outside `run_until`: send completions
are deferred through the event queue, receives fire on delivery. The
sync API throws like the rest of the sim (no link, unknown node,
crashed sender, double bind); async completions report via
`std::error_code`. At most one receive may be pending per socket; a
datagram that arrives with no pending receive waits in a FIFO queue.
Sending from an unbound socket binds an ephemeral port first (from
49152 up, per node); receiving on an unbound socket throws. A
datagram for a port with no socket is silently dropped - the trace
still shows it as delivered, which is how you tell "lost on the
wire" from "arrived, nobody listening". `cancel()` cancels a pending
receive; a crash cancels pending receives and drops queued datagrams,
but the binding survives the restart. Connected UDP (`connect` +
`async_send` / `async_receive`) is supported. See
`examples/udp_echo.cxx`.

Not in this version: a TCP-like reliable stream (that is the next
one), POSIX blocking sockets, fragmentation, multicast, DNS,
socket options, bounded receive buffers, receive timeouts.
