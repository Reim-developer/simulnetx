# Bandwidth

A link can be capacity-limited: `link_config::bandwidth_bps` is the
bytes per second the link can carry, 0 meaning unlimited (the default,
backward compatible). A message of N bytes needs

```
D = floor(N * 8 * 1e9 / bandwidth_bps) nanoseconds
```

on the wire, and messages queue strictly FIFO per directed link: the
second of two back-to-back messages starts only when the first
finishes. Delivery time is finish + latency (+ the jitter sample, +
the reorder extra delay, exactly as without bandwidth).

```cpp
simulnetx::network net(0);
auto& a = net.add_node("A");
auto& b = net.add_node("B");
net.link(a.id(), b.id(), {.bandwidth_bps = 1000});  // 1000 bytes/second

// Three 1000-byte messages sent at t=0 arrive at t=8s, t=16s, t=24s:
// each needs 8s on the wire, and each queues behind the previous one.
a.send(b.id(), std::vector<std::byte>(1000, std::byte{'x'}));
a.send(b.id(), std::vector<std::byte>(1000, std::byte{'x'}));
a.send(b.id(), std::vector<std::byte>(1000, std::byte{'x'}));
net.run_for(30s);
net.delivered_count().equal(3);
```

`scenario::link()` and `link_directed()` take a `link_config`, so
bandwidth flows through the scenario builder unchanged.

What counts as N is the bytes the link actually carries: the user
payload for raw sends, the serialized form (8-byte tag + payload) for
typed sends, the envelope for the UDP/TCP shims. One uniform rule, no
special cases.

Interactions, all pinned by existing rules:

- Loss is decided at send time, before any transmission: a lost
  message consumes no bandwidth, and the next message does not queue
  behind it. A duplicated copy is a second transmission: it consumes
  bandwidth, queues behind the original, and still arrives 1ns after
  the first.
- A message in flight when a partition lands is dropped by the
  delivery check, but its transmission still happened: the link stays
  busy until its finish time, and the next message queues behind it.
- Queueing is link state, not node state: it survives crash/restart,
  and replacing a link's config does not reset its queue.
- Each direction has its own queue: A->B busy does not delay B->A.
- Queueing consumes no RNG: bandwidth is deterministic given arrival
  times and sizes, so the transparency invariant holds.

Limits: bandwidth models capacity, not a queueing discipline. There
are no priorities, no RED/ECN, no congestion control - a link with a
long queue just delivers late, it never drops for being full. And
there is no flow control above the link: a fast sender fills the
queue without pushback. `examples/bandwidth_slow_link.cxx` shows the
three-message spacing above.
