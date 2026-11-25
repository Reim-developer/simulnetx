# Links and faults

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
