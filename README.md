# simulnetx

A deterministic network simulator for C++20. In-process virtual network with a
virtual clock: same seed + same scenario = identical results on every run.
Test distributed protocols and netcode under loss, reorder, partitions and
node crashes with fully reproducible failures. No sockets, no real time, no
threads.

## Quick start

Requires a C++20 compiler and CMake 3.20 or newer.

```sh
git clone https://github.com/Reim-developer/simulnetx.git
cd simulnetx
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
ctest --test-dir build
```

## Example

```cpp
#include "simulnetx/network.hxx"
#include <iostream>

using namespace std::chrono_literals;

int main() {
    simulnetx::network net(42);
    auto& a = net.add_node("A");
    auto& b = net.add_node("B");
    net.link(a.id(), b.id(), {.latency = 50ms});

    b.on_message([](const simulnetx::message& m) {
        std::cout << "B got " << m.payload.size() << " bytes\n";
    });

    a.send(b.id(), "hello");
    net.run_for(1s);

    net.sent_count().equal(1);
    net.delivered_count().equal(1);
    net.dropped_count().equal(0);
}
```

## Scenario

The scenario builder is a fluent facade over `network`: a whole test reads as
one declaration. Each step executes immediately, in chain order.

```cpp
#include "simulnetx/scenario.hxx"

using namespace std::chrono_literals;

int main() {
    simulnetx::scenario("retry under 30% loss")
        .seed(42)
        .nodes("A", "B")
        .link("A", "B", {.latency = 50ms, .loss_rate = 0.30})
        .send_at(0s, "A", "B", "ping")
        .partition_at(5s, {"A"}, {"B"})
        .heal_at(8s)
        .run_until(10s)
        .expect_delivered(1);
}
```

Details (failure messages, escape hatches, topology shorthands) are in
docs/scenario.md.

## Features

- **Links and faults** - latency, jitter, loss, duplication, reorder,
  partitions, crash/restart, directed links. See docs/faults.md.
- **Typed messages** - send values, not bytes; a reflection-based
  serializer, strict tag dispatch, request/response with retries and
  timeouts. See docs/typed-messages.md.
- **Tracing** - an event trace of every run: what was sent, delivered,
  dropped and why. See docs/tracing.md.
- **Socket shim (UDP)** - async Asio-style `udp::socket` on top of the
  sim. See docs/socket-udp.md.
- **Socket shim (TCP)** - a TCP-like reliable stream: `tcp::socket` +
  `tcp::acceptor`, 3-way handshake, retransmission, FIN close.
  See docs/socket-tcp.md.
- **Bandwidth** - capacity-limited links: messages queue FIFO on the
  wire, delivery is finish + latency. Trace export as JSON, and
  opt-in stable type names for cross-compiler tags.
  See docs/bandwidth.md and docs/tracing.md.
- **Stability** - the 1.0.0 freeze: single-thread model, error
  contract, frozen API surface. See docs/stability.md and docs/errors.md.

## License

MIT. See LICENSE.
