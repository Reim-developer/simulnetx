# Typed messages and request/response

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

## Cross-compiler-stable tags

The 8-byte tag is FNV-1a of the type's name, and by default that name
is the mangled `typeid` name - which differs per compiler, so the same
type gets a different tag under g++ and clang++. A typed message
serialized by one cannot be decoded by the other, and golden traces
carrying tags are not comparable across compilers. To fix it for a
type, specialize `type_name` with a stable literal:

```cpp
struct ping { int seq; };

template <> struct simulnetx::type_name<ping> {
    static constexpr std::string_view value = "ping";
};
```

The tag is then FNV-1a of `"ping"` on every compiler. Stability is
opt-in per type: C++ provides no portable type name, so the library
provides the hook. Names must be unique per type (like protobuf type
URLs); two types sharing a name get the same tag by design. Types
without a specialization keep working exactly as before, with their
current tags within one compiler.

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
