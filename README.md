# simulnetx

A deterministic network simulator for C++20. In-process virtual network with a
virtual clock: same seed + same scenario = identical results on every run.
Test distributed protocols and netcode under loss, reorder and partitions with
fully reproducible failures. No sockets, no real time, no threads.

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
one declaration. Each step executes immediately, in chain order; scheduling
before running is the caller's job.

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

The scheduled ping is the message the final check counts. A failed check
throws `simulnetx::assertion_failed` with the scenario name, the seed and the
virtual time in the message:

```
scenario 'retry under 30% loss': expected delivered_count == 1, got 0 (seed 42, t=10000000000ns)
```

`node("A")` and `network()` are escape hatches for full control (handlers,
timers, direct sends, statistics). The builder is a facade, not a test
framework: use it inside whatever test runner you like.

## Loss, reorder and partitions

```cpp
simulnetx::network net(7);
auto& a = net.add_node("A");
auto& b = net.add_node("B");
net.link(a.id(), b.id(), {
    .latency = 10ms,
    .jitter = 5ms,
    .loss_rate = 0.2,
    .dup_rate = 0.1,
    .reorder_rate = 0.3,
});

net.partition({a.id()}, {b.id()});  // cross-group messages are dropped
net.heal();                          // restore delivery

// A failed check throws simulnetx::assertion_failed with the seed and the
// virtual time in the message:
//   expected delivered_count == 1, got 0 (seed 42, t=10500000000ns)
net.delivered_count().equal(1);
```

`examples/retry_under_loss.cxx` shows a full retry protocol (retransmit until
ack) under 30% loss with a partition in the middle of the run, written against
`network` directly. `examples/scenario_retry.cxx` is the same protocol written
with the scenario builder; both produce identical output.

## License

MIT. See LICENSE.
