# Tracing

Every run records an event trace: an ordered log of everything the
simulation did, with virtual timestamps. Counts say a run failed; the
trace says why - which messages were dropped, by loss or by the
partition, in what order the survivors arrived, whether a timer fired.

```cpp
net.run_for(5s);
std::cout << net.dump_trace();  // one event per line, node names resolved
net.event_count(simulnetx::event_kind::dropped).equal(12);
```

Event kinds: `sent`, `delivered`, `dropped` (with a reason: `loss`,
`partition`, `crashed`), `duplicated`, `reordered`, `timer_fired`,
`node_crashed`, `node_restarted`, `partitioned`, `healed`, plus the
typed layer's `request_sent`, `response_received`, `request_timeout`.
Each event carries its virtual time, the nodes involved, the payload
size and, for typed messages, the type tag. Payload content is not
recorded.

```cpp
// The vector, for filtering with standard algorithms:
for (const auto& e : net.trace()) {
    if (e.kind == simulnetx::event_kind::dropped &&
        e.reason == simulnetx::drop_reason::partition) {
        std::cout << net.format_event(e) << '\n';
    }
}
// Streaming dump, for traces too large to hold as one string:
net.dump_trace(std::cout);
```

Tracing is on by default and is a pure observer: recording consumes no
RNG draws and never changes scheduling, counts or deliveries, so the
same seed + scenario produces identical stats with tracing on or off.
The same seed + scenario also produces an identical trace vector and
an identical dump string. `set_tracing(false)` stops recording,
`clear_trace()` empties the trace. Timers return a `timer_id`
(`network::after`, `schedule_at`, `node::after`) that appears in their
`timer_fired` events.

The scenario builder exposes the same API: `scenario::trace()`,
`scenario::dump_trace()`, `expect_events(kind, n)` and
`expect_events_between(kind, lo, hi)`.
