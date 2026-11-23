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

## Typed messages

Send values, not bytes. Any serializable type works: trivially-copyable
types, `std::string`, `std::vector`, `std::optional`, `std::variant`,
`std::pair`, `std::tuple`, `std::array`, and plain aggregates - even with
`std::string` or `std::vector` members - with no boilerplate: the
serializer reflects over the fields. A type that is not serializable fails
at compile time via the `serializable` concept.

```cpp
struct hello { std::string name; };
struct world { std::string text; };

simulnetx::network net(42);
auto& a = net.add_node("A");
auto& b = net.add_node("B");
net.link(a.id(), b.id(), {});

b.on<hello>([](const hello& h) { /* h.name == "reim" */ });
b.on<world>([](const world& w) { /* ... */ });
a.send<hello>(b.id(), hello{"reim"});
net.run_for(1s);
```

Each typed message carries an 8-byte type tag, so `on<T>` dispatches by
type. `msg.as<T>()` decodes a message by hand and throws
`simulnetx::decode_error` on a tag mismatch, truncation, or trailing
bytes: a decode either reproduces the sent value exactly or throws.

Strict tag dispatch is the default: a node with typed handlers that
receives a message no handler matches throws `simulnetx::unhandled_message`
out of `run_until` (node name, tag, seed and virtual time in the message).
`on_message` is the catch-all - a message no typed handler matches falls
through to it - and nodes with no handlers at all keep silently ignoring
deliveries. Opt out with `net.set_strict_tags(false)`, or
`.lenient_tags()` in a scenario.

A non-aggregate type opts in by specializing the serializer trait:

```cpp
template <> struct simulnetx::serializer<money> {
    static void encode(const money& m, std::vector<std::byte>& out);
    static money decode(simulnetx::detail::cursor& in);
};
```

The reflection engine is public too: `simulnetx::reflect::field_count`,
`get`, `for_each_field` and `to_tuple` work on any aggregate up to 200
fields. Limits: no C-array members (use `std::array`), no reference
members, no base classes with members, no more than 200 fields. Each limit
fails loudly at compile time.

## Request/response

`serve` registers a handler that answers requests of one type;
`request` sends a typed request and returns a handle for retries, a
timeout, and the callbacks:

```cpp
simulnetx::scenario sc("typed ping/pong");
sc.seed(42)
  .nodes("client", "server")
  .link("client", "server", {.latency = 50ms, .loss_rate = 0.30})
  .serve<hello, world>("server", [](const hello& h) {
      return world{"hello, " + h.name};
  });

sc.request<hello, world>("client", "server", hello{"reim"})
  .retry_every(1s).timeout_after(10s)
  .on_response([](const world& w) { /* w.text == "hello, reim" */ })
  .on_timeout([] { /* no answer in 10s */ });

sc.run_until(10s);
```

The first request fires immediately. Retries reuse the same request id;
the first response wins, so duplicates and late responses are ignored
(at-most-once delivery to the callback). A timeout removes the pending
entry and calls `on_timeout`. If the requesting node crashes, its pending
requests are cancelled silently: a dead node's callbacks never fire. A
server that never replies just lets the client time out.
`examples/typed_ping_pong.cxx` is this example as a runnable program:

```
answer: hello, reim, arrived at t=4100000000ns, server served 4 request(s)
sent=9 delivered=5 dropped=4
```

## Topology

Shorthand for common link layouts, operating on the nodes added so far, in
insertion order:

```cpp
simulnetx::scenario("cluster")
    .seed(1)
    .nodes("A", "B", "C", "D")
    .mesh({.latency = 10ms});       // every pair linked
    // .star("A", {...});           // A linked to every other node
    // .chain({...});               // A <-> B <-> C <-> D
```

`mesh` and `chain` need at least 2 nodes, `star` needs a hub plus at least
1 other node; fewer throws `std::logic_error`. `mesh` also takes an
explicit list of names to link a subset.

## License

MIT. See LICENSE.
