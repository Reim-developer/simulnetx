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

## JSON export

The same trace as JSON, for machines rather than eyes: `dump_trace_json()`
returns one object, compact, on a single line:

```json
{"seed": 42, "events": [{"at": 0, "kind": "sent", "from": 0, "to": 1, "bytes": 2, "tag": 0, "reason": "loss", "timer": 0, "node": 0, "request_id": 0, "attempt": 0}, ...]}
```

Each event is an object with all eleven `trace_event` fields in
declaration order. Times are nanosecond counts; kinds and drop reasons
are the text-format names; tags are JSON numbers. Fields irrelevant to
an event's kind still appear, with their defaults (a sent event shows
`"reason": "loss"`): the shape is uniform, so a parser never has to
guess which keys exist. One caveat: consumers that parse numbers as
IEEE doubles (notably JavaScript) lose precision above 2^53 - the tag
is the field where that matters, so compare tags as strings after
parsing if exactness is needed. The text `dump_trace()` format is
unchanged, and exporting never affects outcomes, like tracing itself.
`dump_trace_json(std::ostream&)` streams incrementally, for traces too
large to hold as one string. The scenario builder exposes the same
call as `scenario::dump_trace_json()`.

The scenario builder exposes the same API: `scenario::trace()`,
`scenario::dump_trace()`, `expect_events(kind, n)` and
`expect_events_between(kind, lo, hi)`.
