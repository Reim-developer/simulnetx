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

## Crash and restart

A crashed node drops incoming messages, cannot send or schedule timers, and
loses its pending timers (they belong to the dead incarnation and are never
restored). Its message handler survives the restart, and a message arriving
after the restart is delivered.

```cpp
simulnetx::network net(1);
auto& a = net.add_node("A");
auto& b = net.add_node("B");
net.link(a.id(), b.id(), {});

net.crash(b.id());
a.send(b.id(), "x");  // dropped: the destination is down
net.restart(b.id());
a.send(b.id(), "y");  // delivered: B is back
net.run_for(1s);

net.delivered_count().equal(1);
net.dropped_count().equal(1);
```

In a scenario, `.crash_at(5s, "A")` and `.restart_at(8s, "A")` schedule the
outage. `examples/failover.cxx` shows a full primary/backup heartbeat
failover: A crashes, B takes over on missed heartbeats, A restarts and
rejoins.

## Directed links

`link()` sets the same config in both directions. `link_directed()` sets one
direction only; the reverse direction is unchanged (and still has no link if
none was set).

```cpp
simulnetx::network net(1);
auto& a = net.add_node("A");
auto& b = net.add_node("B");
net.link_directed(a.id(), b.id(), {.latency = 10ms});

a.send(b.id(), "x");  // delivered
// b.send(a.id(), "y");  // throws std::invalid_argument: no link B -> A
net.run_for(1s);

net.delivered_count().equal(1);
```

## License

MIT. See LICENSE.
