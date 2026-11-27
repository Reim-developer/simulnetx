# Stability

What 1.0.0 freezes, and what it does not. The freeze is a contract
with a checkable proof (see "Proof" below), not a marketing claim.

## The frozen surface

Everything public as of v0.8.0, and nothing else:

- Every public header under `include/simulnetx/` except `detail::`
  (anything in a `detail` namespace is internal and may change in any
  release, including a patch).
- Every public function, type, constant and enum in those headers:
  `tests/api_surface_test.cxx` includes every header and calls every
  public function once at smoke level. If a function is not in that
  file, it is not frozen - it is a bug in the test.
- The text format of `network::dump_trace()` and the JSON shape of
  `network::dump_trace_json()`: field names, event kind names
  (`event_kind_name`), drop reason names (`drop_reason_name`) and the
  order of events are frozen. New event kinds and new fields may be
  added only in a major release.
- The error contract in `docs/errors.md`: the three channels (sync
  throws, async `std::error_code`, compile-time concepts) and which
  exception type each call throws.
- The numeric tag of a type is frozen ONLY when the type has a
  `type_name<T>` specialization (a string literal). The tag of a type
  WITHOUT a specialization is `typeid(T).name()` and is NOT frozen:
  it differs across compilers and may change with a compiler upgrade.

What is explicitly NOT frozen: `detail::` namespaces, private
members, the internal layout of any type, and the exact wording of
failure messages (only the exception TYPE and the seed + virtual time
in the message are guaranteed).

## Single-thread model

One virtual clock, no threads, no locks, no atomics, no global
mutable state. Verified by audit at 1.0.0 (grep over `include/` and
`src/`), not by prose:

- No `<thread>`, `<mutex>`, `<atomic>` or `std::async` anywhere in the
  library. No locks, no atomics.
- No `time()`, `rand()`, `std::random_device`, `getenv` or any
  wall-clock read anywhere in the library. The only entropy source is
  the seeded splitmix64 RNG, consumed only where the docs say
  (loss, duplication, reorder and jitter sampling). Bandwidth,
  queueing, tracing and JSON export consume no RNG.
- No non-const `static` or namespace-scope mutable state. The only
  statics in the library are: `static` member functions (stateless
  encode/decode in `serializer.hxx`), `static constexpr` lookup
  tables, and the `type_name` primary's `inline static const`
  `std::string_view` (a constant, not mutable state). All mutable
  state lives in `network` instances (plus the per-connection shared
  state the registry owns). Two networks in one process are fully
  independent - proven by
  `DeterminismTest.TwoNetworksWithSameSeedAreIndependent`: two
  networks, same seed, interleaved runs, identical traces.
- Handlers run ONLY inside `run_until`. Calling `run_until`
  (or `run_for`, which calls it) reentrantly from a handler throws
  `std::logic_error` out of the outer `run_until`, and the network
  stays usable afterwards (the guard resets even when the throw
  escapes the handler). Sending, scheduling timers and arming
  receives from inside a handler are legal and common (replies,
  retries) - only reentering the run loop is forbidden.

## Determinism

Same seed + same scenario = identical results, on every run, on
every compiler. That means: the same delivery times, the same
counts, the same trace (byte-identical `dump_trace()` and
`dump_trace_json()`), the same example outputs. The proof is the
test suite plus the 8 examples, byte-identical across 2 runs x 2
compilers (g++ and clang). Tracing off changes nothing: with tracing
disabled the run is identical (no RNG consumed, no timing change),
proven by the tracing-off transparency tests.

## Known limits (stay limits in 1.0.0, no fix)

- JSON doubles above 2^53 lose precision in double-based parsers.
  That is a property of the parsers, not of the export.
- `field_names<T>` is hand-written per struct: C++20 cannot reflect
  field names, so `json::to_json` needs the explicit table.
- `type_name<T>` stability is opt-in per type: without a
  specialization the tag is `typeid(T).name()` and varies by
  compiler.
- Bandwidth is FIFO with no priorities, no RED/ECN, no congestion
  control.
- No POSIX blocking sockets: the socket shims are async-only, on top
  of the sim.
- No clock skew between nodes; no multi-process or distributed sim:
  one process, one virtual clock.
- FIN_WAIT_2: a socket whose peer never sends FIN is unregistered
  after a 60s silent timeout (see docs/socket-tcp.md). The timeout
  frees your side of the 4-tuple only; the peer's side still owns its
  half, exactly as in real TCP.
- The virtual clock is int64 nanoseconds: a run cannot pass
  int64 max. A send whose delivery would overflow the clock throws
  `std::invalid_argument` (see docs/errors.md).

## Proof (checkable)

- Full test suite green on all 3 CI compilers (g++, clang, MSVC).
- `tests/api_surface_test.cxx`: every public header included, every
  public function called once at smoke level.
- All 8 examples byte-identical across 2 runs x 2 compilers.
- Two-network independence test green.
- Tracing-off transparency re-verified.
- Every public function has a doc comment in its header.
- `docs/` covers every feature: core, scenario, faults,
  typed-messages, tracing, socket-udp, socket-tcp, bandwidth.
- `docs/errors.md` and this file exist and are linked from the
  README Features section.
