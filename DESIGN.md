# simulnetx design notes (v0.1.0)

## What it is

simulnetx is an in-process simulated network with a virtual clock, for testing
distributed protocols and netcode under bad networks (loss, reorder,
partitions) with fully reproducible failures. Same seed + same scenario =
identical results on every run, on every platform.

It is pure computation: no real threads, no real time, no sockets, no OS
calls. Everything runs on any platform with a C++20 compiler.

## API rationale

- **Message-handler style, not a socket shim.** In v0.1.0, protocol code plugs
  in through `node::on_message`, `node::send` and `node::after`. A socket-shim
  API (same code against real sockets and the sim) is more powerful but needs
  a much larger surface; it is deferred.
- **Stable node references.** Nodes live in `unique_ptr`s inside a vector, so
  `add_node` never invalidates references returned earlier. Protocol code can
  keep `node&` across the whole scenario.
- **Raw byte payloads.** `message::payload` is `vector<std::byte>`; the sim
  does not interpret content. `send` has a `string_view` convenience overload
  for text protocols.
- **Fluent assertions on sim observables.** `sent_count()`, `delivered_count()`
  and `dropped_count()` return `expectation<size_t>`, so a scenario reads as
  `net.delivered_count().equal(1)`. A failed check throws `assertion_failed`
  with expected vs actual AND the seed AND the current virtual time in the
  message, so the failure itself is the reproduction recipe.
  `simulnetx::expect(value)` wraps arbitrary values the same way, without sim
  context.

## Determinism contract

Two rules make results reproducible:

1. **Event order is total.** The event queue is a priority queue ordered by
   (virtual time, monotonically increasing sequence number). The sequence
   number breaks every tie, so two events at the same virtual time always
   process in scheduling order.
2. **All randomness comes from the hand-rolled splitmix64 RNG.** The standard
   does NOT fix the output of `std::uniform_*_distribution` across standard
   libraries, so the same seed would produce different results under
   libstdc++ and libc++. simulnetx never uses them for anything affecting
   simulation outcomes. Uniform doubles come from the top 53 bits of the RNG
   output divided by 2^53, which is exact on all IEEE 754 doubles.

Sampling order per send is fixed: loss, jitter, dup, reorder. A draw happens
only when the corresponding feature is active (rate > 0 or jitter > 0), so
enabling one feature does not change the stream of the others. A message
dropped by a partition consumes no RNG draws. Golden tests pin exact outputs
for fixed seeds; any change to the RNG, the draw order, or the sampling math
breaks them on purpose and must be re-pinned consciously.

## Event model

- Single-threaded discrete-event loop. `run_until(t)` processes all events
  with time <= t in (time, sequence) order, then sets the virtual clock to t.
  `run_for(d)` is `run_until(now + d)`.
- Timers are events. A timer callback may send messages and schedule further
  timers; those events are ordered by the same (time, sequence) rule.
- Self-send delivers at the current virtual time with no link and no RNG
  draws. It is a loopback, not a link.
- A node with no handler silently ignores delivered messages; they still count
  as delivered.

## Link semantics

- **latency**: base one-way delay added to every delivery.
- **jitter**: an extra uniform [0, jitter] delay sampled per message.
- **loss**: the message is dropped (counted as dropped) and never scheduled.
- **dup**: a second identical copy is delivered 1ns after the first (counted
  as an extra delivery).
- **reorder**: the message gets a second uniform [0, jitter] delay, so it may
  arrive after a later-sent message. No visible effect when jitter is zero.
- **partition**: checked at send time (message never enters the network) and
  at delivery time (a message in flight when the partition starts is lost).
  Both count as dropped. Nodes in neither group are unaffected.

Stats invariant: delivered + dropped == sent + (number of duplicate copies).

## Deferred

- **Bandwidth limits**: per-link bytes-per-second with queueing. Needs a
  queueing model for in-flight bytes; v0.1.0 has unbounded links.
- **Node crash/restart**: nodes that stop processing and resume, with
  in-flight messages to a crashed node dropped. Needs a node lifecycle API.
- **Directed (asymmetric) links**: separate config per direction. v0.1.0
  links are symmetric; the key type already supports a directed variant.
- **Socket-shim API**: a fake socket layer mirroring real sockets, so the same
  protocol code runs against real sockets in production and the sim in tests.
  This is the big one; it needs the message-handler core to be proven first.

## Notes

- `network::now()` is a small addition beyond the original v0.1.0 brief: timer
  and handler code needs to observe the virtual time, and there is no other
  way to expose it.
