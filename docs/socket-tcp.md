# Socket shim (TCP)

A TCP-like reliable stream over the sim: `simulnetx::tcp::socket` and
`simulnetx::tcp::acceptor`, async Asio-style, beside `simulnetx::udp`.
An address is a node plus a port (TCP and UDP ports are separate
registries, like a real stack). The stream is bytes, not messages:
writing `"hello"` then `"world"` may read back as `"helloworld"` in
one `async_read_some`. The handshake is 3-way (SYN, SYN+ACK, ACK),
sequence numbers start at 0, the MSS is 1024 bytes, and lost segments
are retransmitted on a 200ms doubling RTO (max 5 retransmits, then
`timed_out`). RTO timers record nothing and consume no timer id.

```cpp
#include "simulnetx/tcp.hxx"

simulnetx::tcp::acceptor acceptor(net, server_node);
acceptor.bind(9000);
acceptor.listen(8);
acceptor.async_accept([](simulnetx::tcp::socket s, std::error_code ec) {
    // s is the accepted connection; read from it, write back to it.
});

simulnetx::tcp::socket client(net, client_node);
client.async_connect({server_node.id(), 9000},
                     [&client](std::error_code ec) {
                         client.async_write("ping", [](auto, auto) {});
                     });
```

No completion handler ever runs outside `run_until`: connect, write,
read and accept completions are all deferred through the event queue.
The sync API throws like the rest of the sim (no link, unknown node,
double bind, unconnected socket); async completions report via
`std::error_code`. A SYN to a port with no listener gets an RST and
the connect reports `connection_refused`. At most one read and one
accept may be pending; writes may queue and complete in order. A
write copies its data; a read buffer is caller-owned. `close()`
sends a FIN after the queued writes; the peer's FIN makes the next
read complete with `(success, 0)` - EOF, not an error. A read stays
legal after `close()` (half-close). The port is rebound immediately
after close (no TIME_WAIT), but the 4-tuple stays registered while
the connection is in FIN_WAIT_2: a new connect from the same port to
the same peer throws `logic_error` until the peer sends its FIN or
the 60s FIN_WAIT_2 timeout frees it (after the timeout the connect
registers, then times out - the peer's side still owns its half of
the 4-tuple, exactly as in real TCP). A crash cancels the crashed
node's sockets' pending ops, but the binding survives the restart;
the peer learns via RST after the restart (`connection_reset`). See
`examples/tcp_echo.cxx`: 7 segments, 14 trace events, reply at
t=80ms at 20ms latency.

Not in this version: flow/congestion control, SACK, Nagle,
keepalive, TIME_WAIT, a scenario API (use `scn.network()` +
`scn.node("name")`), POSIX blocking sockets.
