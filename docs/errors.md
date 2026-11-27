# Errors

The error contract, frozen in 1.0.0. There are exactly three channels, no
others. Every row below is pinned by a test (named in the last column);
a row whose test is missing is a bug in the test suite, not a gap in the
contract.

## Channel 1 - synchronous throws

The sync API throws like the rest of the sim. Two standard types only,
plus three named simulnetx types (caught by name):

- `std::invalid_argument` - bad arguments: a value the caller passed
  cannot be used as given.
- `std::logic_error` - state violations: the call is well-formed but the
  object (or the network) is in a state that forbids it.
- `decode_error : std::invalid_argument` - a failed decode means the
  input bytes were bad arguments to the decoder (truncated, tag
  mismatch, malformed). It derives from `std::invalid_argument` so a
  catch of either type sees it.
- `assertion_failed : std::runtime_error` and
  `unhandled_message : std::runtime_error` - named diagnostic types for
  two sim-specific failures (a failed check; a strict-mode unmatched
  message). They derive from `std::runtime_error` because they are
  diagnostic signals, not argument or state errors, and users catch them
  by name - every test in the suite does. No *bare* `std::runtime_error`
  or `std::system_error` is ever thrown: the only exception types that
  escape the public API are `std::invalid_argument`,
  `std::logic_error`, and the three named types above.

### std::invalid_argument

| Call | Condition | Test |
|---|---|---|
| `network::link`, `link_directed` | unknown node id; self-link; latency or jitter negative; a rate outside [0, 1] | network_test, directed_test |
| `network::partition` | unknown node id; a node in both groups | partition_test |
| `network::crash`, `restart`, `is_crashed` | unknown node id | crash_test |
| `network::run_until` | target before the current virtual time | network_test |
| `network::run_for` | negative duration | network_test |
| `network::after` | negative delay | network_test |
| `network::schedule_at` | time before the current virtual time | network_test |
| `network::schedule_node_timer` (via `node::after`) | negative delay | network_test |
| `node::send`, `node::send<T>`, `node::request` | send to an unknown node id; no link to the destination | network_test, directed_test, typed_test, request_test |
| `node::send`, `node::request` (via `send_from`) | virtual time would overflow int64 nanoseconds (delivery past the end of the clock) | errors_test |
| `network::transmit_at` (via `send_from`, bandwidth links) | serialization delay overflows int64 nanoseconds | not pinned: reachable only with a payload above ~1.1 GB at 1 Bps, so the guard is defensive - see the note below |
| `json::dump` | non-finite double (NaN or infinity) | json_test |
| `request_handle::retry_every` | non-positive interval | request_test |
| `request_handle::timeout_after` | non-positive timeout | request_test |
| `scenario::nodes` | duplicate node name | scenario_test |
| `scenario` (any name lookup) | unknown node name | scenario_test, topology_test |
| `scenario::partition_at` | a name in both groups; time in the past | scenario_test |
| `scenario::heal_at`, `crash_at`, `restart_at`, `send_at`, `schedule_at` | time in the past | scenario_test |
| `scenario::link`, `link_directed` | invalid config or self-link (via `network::link`) | scenario_test |
| `scenario::mesh(names)` | unknown name | topology_test |

### decode_error (derives from std::invalid_argument)

| Call | Condition | Test |
|---|---|---|
| `message::as<T>` | payload too short for a type tag; tag mismatch (sent as a different type, or raw); trailing bytes after the value | typed_test |
| `serializer<T>::decode` | truncated payload (`cursor::take` overrun); invalid optional flag; variant index out of range | serializer_test |
| `node::request` / `serve` reply path | trailing bytes after the request or response value | request_test |
| `node::request` response path | response tag mismatch for the request id | request_test |

### std::logic_error

| Call | Condition | Test |
|---|---|---|
| `network::run_until` | reentrant call from a handler (see docs/stability.md) | errors_test |
| `network::send_from` (via `node::send`) | send from a crashed node | crash_test |
| `network::schedule_node_timer` (via `node::after`) | schedule on a crashed node | crash_test |
| `network::register_tcp_connection` | a TCP connection for this 4-tuple already exists (reconnect while the old connection still lives) | tcp_test (FIN_WAIT_2 timeout test) |
| `network::register_socket` (via `udp::socket::bind`) | port already bound on the node | socket_test |
| `network::allocate_ephemeral_port` (via `udp::socket::bind(0)`) | no ephemeral ports left on the node (all 16384 bound) | errors_test |
| `network::claim_tcp_port` (via `tcp::socket::bind`, `acceptor::bind`) | port already claimed on the node | errors_test |
| `network::allocate_tcp_ephemeral_port` (via `tcp::socket::bind(0)`) | no TCP ephemeral ports left on the node | errors_test |
| `scenario::seed` | called after the network was created | scenario_test |
| `scenario::mesh`, `mesh(names)`, `star`, `chain` | too few nodes (mesh/chain need >= 2, star needs a hub plus >= 1 other) | topology_test |
| `request_handle::retry_every`, `timeout_after` | called twice on the same request | request_test |
| `udp::socket::bind` | on a closed socket; already bound | errors_test, socket_test |
| `udp::socket::local_endpoint` | on an unbound socket | socket_test |
| `udp::socket::connect` | on a closed socket | errors_test |
| `udp::socket::async_send_to` | on a closed socket | errors_test |
| `udp::socket::async_send` | on an unconnected socket | socket_test |
| `udp::socket::async_receive_from` | on a closed or unbound socket; a receive already pending | errors_test, socket_test |
| `udp::socket::async_receive` | on an unconnected socket | errors_test |
| `tcp::socket::bind` | on a closed socket; already bound | errors_test |
| `tcp::socket::local_endpoint` | on an unbound socket | errors_test |
| `tcp::socket::remote_endpoint` | on an unconnected socket | errors_test |
| `tcp::socket::async_connect` | on a closed socket; on a connected socket | errors_test |
| `tcp::socket::async_write` | on a closed or unconnected socket | errors_test, tcp_test |
| `tcp::socket::async_read_some` | on an unconnected socket; a read already pending | errors_test |
| `tcp::socket` (any operation) | on a moved-from socket | errors_test |
| `tcp::acceptor::bind` | on a closed acceptor; already bound | errors_test |
| `tcp::acceptor::listen` | on a closed acceptor; on an unbound acceptor; already listening | errors_test |
| `tcp::acceptor::local_endpoint` | on an unbound acceptor | errors_test |
| `tcp::acceptor::async_accept` | on a closed acceptor; on a non-listening acceptor; an accept already pending | errors_test |

### assertion_failed and unhandled_message (named, : std::runtime_error)

| Thrown by | Condition | Test |
|---|---|---|
| `expectation<T>` checks (`equal`, `between`, ...), `scenario::expect_*` | expected vs actual mismatch (scenario failures prefix the scenario name) | expectation_test, scenario_test |
| `network::deliver` (inside `run_until`) | strict mode (the default): a message reaches a node with typed handlers, no handler matches, and no `on_message` catch-all is registered. The message carries node name, wire tag, seed and virtual time. Nodes with no typed handlers are unaffected. Call `network::set_strict_tags(false)` (or `scenario::lenient_tags()`) to ignore unmatched messages instead | typed_test, trace_test |

Note on the two unpinned guards: `network::schedule_silent_at` throws
`std::invalid_argument` for a time in the past, but it is private -
only the TCP shim's retransmission timers call it, always with
`now() + delay`, so no public call can reach the throw. The
serialization-delay overflow guards in `network.cxx` are defensive
arithmetic: the `whole`-part guard needs a payload above ~1.1 GB at
1 Bps, and the remainder guards need bandwidths above ~1.8e13 Bps, so
no test pins them (a gigabyte allocation to prove a guard is coverage
theater, not proof). The reachable overflow - a delivery time past the
end of the virtual clock - is pinned by the errors_test case that runs
the clock to near int64 max.

## Channel 2 - async completions via std::error_code

Async errors never throw. Handlers report through `std::error_code`
with `std::errc` values only - no custom error category (standing rule
since v0.6.0). Success is a clear (default-constructed) `error_code`.

| Code | Meaning | Test |
|---|---|---|
| `connection_refused` | connect/accept to a port with no listener (RST) | tcp_test |
| `timed_out` | RTO exhaustion: the handshake or a retransmit gave up after 5 retries (12.6s) | tcp_test |
| `operation_canceled` | the socket or acceptor was closed, or its node crashed, while an operation was pending; a node timer was cancelled at crash | socket_test, tcp_test, crash_test |
| (success, 0 bytes) | EOF on a read: the peer sent FIN and the buffered bytes are drained. EOF is a success with 0 bytes, not an error - there is no portable eof in `std::errc` | tcp_test |

Two distinctions to keep straight: a request timeout is reported
through the request API's `on_timeout` callback, not as an
`error_code` (see `request_handle`); and a send-path failure (no link,
unknown node, crashed sender) throws synchronously out of
`async_send_to` / `async_send` / `async_connect` / `async_write` - the
handler never fires - because it is a sync error (channel 1), not an
async one.

## Channel 3 - compile-time errors

Concepts and `static_assert`s fail the build, not the run. They are
part of the contract too: a failure here means the code violates the
API, not that the sim found a bug.

| Failure | Why |
|---|---|
| `serializable<T>` not satisfied | `node::send<T>`, `on<T>`, `request`, `serve` need a type the serializer can encode: an aggregate (via reflection), a trivially-copyable type, a built-in container, or a user `serializer<T>` specialization. Pointers, member pointers and C arrays are excluded. |
| `reflectable<T>` not satisfied | `reflect::field_count`, `to_tuple`, `get`, `for_each_field` need an aggregate with no unions, no reference members, at most 200 fields, no C-style array members (use `std::array`). A reference member with a default member initializer, not in first position, passes the concept and fails later with a structured-binding count mismatch - see reflect.hxx. |
| `field_names<T>` missing | `json::to_json` on a reflectable type without a `field_names` specialization is a `static_assert`: C++20 cannot reflect field names. |
| `type_name<T>` misuse | Specialize with a string literal for cross-compiler tag stability; the primary uses `typeid(T).name()` and is not `constexpr` (see serializer.hxx). |
