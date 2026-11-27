// ApiSurfaceTest: the frozen 1.0.0 surface (see docs/stability.md).
// Includes every public header and calls every public function once at
// smoke level (construct, call, destroy). It is a test, not an example:
// no output contract beyond exit 0. Functions in detail:: namespaces
// and private machinery are deliberately NOT covered - they are not
// frozen.
#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <functional>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <system_error>
#include <tuple>
#include <utility>
#include <variant>
#include <vector>

#include <gtest/gtest.h>

#include "simulnetx/errors.hxx"
#include "simulnetx/expectation.hxx"
#include "simulnetx/json.hxx"
#include "simulnetx/link_config.hxx"
#include "simulnetx/message.hxx"
#include "simulnetx/network.hxx"
#include "simulnetx/node.hxx"
#include "simulnetx/reflect.hxx"
#include "simulnetx/request.hxx"
#include "simulnetx/rng.hxx"
#include "simulnetx/scenario.hxx"
#include "simulnetx/serializer.hxx"
#include "simulnetx/tcp.hxx"
#include "simulnetx/trace.hxx"
#include "simulnetx/udp.hxx"

namespace {

using namespace std::chrono_literals;
using namespace simulnetx;

struct ping {
    std::string text;
    int n{};
};
struct pong {
    std::string text;
    int n{};
};

std::vector<std::byte> as_bytes(std::string_view text) {
    std::vector<std::byte> out;
    out.reserve(text.size());
    for (const char c : text) {
        out.push_back(static_cast<std::byte>(c));
    }
    return out;
}

std::string as_string(const std::byte* data, std::size_t n) {
    std::string out;
    out.resize(n);
    if (n > 0) {
        std::memcpy(out.data(), data, n);
    }
    return out;
}

}  // namespace

namespace simulnetx::json {

template <>
struct field_names<ping> {
    static constexpr std::array<std::string_view, 2> value = {"text", "n"};
};

template <>
struct field_names<pong> {
    static constexpr std::array<std::string_view, 2> value = {"text", "n"};
};

}  // namespace simulnetx::json

namespace {

TEST(ApiSurfaceTest, NetworkCore) {
    network net(42);
    node& a = net.add_node("A");
    node& b = net.add_node("B");
    EXPECT_EQ(a.id(), 0u);
    EXPECT_EQ(b.id(), 1u);
    EXPECT_EQ(a.name(), "A");
    EXPECT_EQ(b.name(), "B");

    net.link(a.id(), b.id(), {.latency = 10ms, .jitter = 1ms, .loss_rate = 0.0,
                              .dup_rate = 0.0, .reorder_rate = 0.0,
                              .bandwidth_bps = 0});
    net.link_directed(b.id(), a.id(), {.latency = 5ms});
    EXPECT_TRUE(net.strict_tags());
    net.set_strict_tags(false);
    EXPECT_FALSE(net.strict_tags());
    net.set_strict_tags(true);
    EXPECT_TRUE(net.strict_tags());

    const timer_id tid = net.after(10ms, [] {});
    EXPECT_EQ(tid, 0u);
    net.schedule_at(20ms, [] {});
    EXPECT_EQ(net.now().count(), 0LL);

    bool saw = false;
    b.on_message([&](const message&) { saw = true; });
    a.send(b.id(), as_bytes("hi"));
    a.send(b.id(), "yo");
    net.run_until(50ms);
    EXPECT_TRUE(saw);
    EXPECT_EQ(net.now().count(), 50'000'000LL);
    net.run_for(50ms);
    EXPECT_EQ(net.now().count(), 100'000'000LL);

    // expectation<T>: implicit conversion plus every check.
    const std::size_t sent = net.sent_count();
    EXPECT_EQ(sent, 2u);
    net.sent_count().equal(2).not_equal(0).greater_than(1).less_than(3);
    net.sent_count().greater_or_equal(2).less_or_equal(2).between(2, 2);
    net.delivered_count().equal(2);
    net.dropped_count().equal(0);
    net.event_count(event_kind::sent).equal(2);
    net.event_count(event_kind::delivered).equal(2);

    // Trace: live view, dumps (both forms), one-event format.
    EXPECT_FALSE(net.trace().empty());
    EXPECT_FALSE(net.dump_trace().empty());
    EXPECT_FALSE(net.dump_trace_json().empty());
    std::ostringstream text_out;
    net.dump_trace(text_out);
    EXPECT_FALSE(text_out.str().empty());
    std::ostringstream json_out;
    net.dump_trace_json(json_out);
    EXPECT_FALSE(json_out.str().empty());
    EXPECT_FALSE(net.format_event(net.trace().front()).empty());

    EXPECT_TRUE(net.tracing());
    net.set_tracing(false);
    EXPECT_FALSE(net.tracing());
    net.set_tracing(true);
    net.clear_trace();
    EXPECT_TRUE(net.trace().empty());

    net.partition({a.id()}, {b.id()});
    net.heal();
    EXPECT_FALSE(net.is_crashed(a.id()));
    net.crash(a.id());
    EXPECT_TRUE(net.is_crashed(a.id()));
    net.restart(a.id());
    EXPECT_FALSE(net.is_crashed(a.id()));

    // splitmix64: same seed, same stream; doubles in [0, 1).
    EXPECT_EQ(splitmix64(42).next_u64(), splitmix64(42).next_u64());
    const double d = splitmix64(42).next_double();
    EXPECT_GE(d, 0.0);
    EXPECT_LT(d, 1.0);
}

TEST(ApiSurfaceTest, NodeMessaging) {
    network net(7);
    node& a = net.add_node("A");
    node& b = net.add_node("B");
    net.link(a.id(), b.id(), {.latency = 10ms});

    ping last{};
    bool typed_saw = false;
    b.on<ping>([&](const ping& p) {
        typed_saw = true;
        last = p;
    });
    a.send<ping>(b.id(), ping{"hi", 3});
    net.run_for(50ms);
    EXPECT_TRUE(typed_saw);
    EXPECT_EQ(last.text, "hi");
    EXPECT_EQ(last.n, 3);

    // Node timer.
    bool timer_fired = false;
    const timer_id tid = a.after(10ms, [&] { timer_fired = true; });
    (void)tid;
    net.run_for(20ms);
    EXPECT_TRUE(timer_fired);

    // Request/response with every request_handle call.
    b.serve<ping, pong>([](const ping& p) { return pong{p.text, p.n + 1}; });
    bool got_response = false;
    bool got_timeout = false;
    auto handle = a.request<ping, pong>(b.id(), ping{"req", 1});
    handle.retry_every(100ms)
        .timeout_after(1s)
        .on_response([&](const pong& p) {
            got_response = true;
            EXPECT_EQ(p.n, 2);
        })
        .on_timeout([&] { got_timeout = true; });
    net.run_for(500ms);
    EXPECT_TRUE(got_response);
    EXPECT_FALSE(got_timeout);

    // Lenient mode: an unmatched typed message is ignored, not thrown.
    // (b has a typed ping handler and no on_message catch-all.)
    net.set_strict_tags(false);
    a.send<pong>(b.id(), pong{"unmatched", 0});
    EXPECT_NO_THROW(net.run_for(50ms));

    // message::as<T> on a captured message, on a node with no typed
    // handlers (so on_message sees every delivery).
    network net2(8);
    node& c = net2.add_node("C");
    node& d = net2.add_node("D");
    net2.link(c.id(), d.id(), {.latency = 10ms});
    std::uint64_t last_tag = 0;
    d.on_message([&](const message& m) { last_tag = m.tag; });
    c.send<ping>(d.id(), ping{"yo", 4});
    net2.run_for(50ms);
    EXPECT_EQ(last_tag, detail::type_tag<ping>());
    ping decoded{};
    d.on_message([&](const message& m) { decoded = m.as<ping>(); });
    c.send<ping>(d.id(), ping{"yo", 4});
    net2.run_for(50ms);
    EXPECT_EQ(decoded.text, "yo");
    EXPECT_EQ(decoded.n, 4);
}

TEST(ApiSurfaceTest, Scenario) {
    // Core chain + expectations + dumps.
    scenario scn("api tour");
    EXPECT_EQ(scn.name(), "api tour");
    scn.seed(7)
        .nodes("A", "B")
        .link("A", "B", {.latency = 10ms})
        .send_at(0s, "A", "B", "hi")
        .run_until(100ms)
        .expect_sent(1)
        .expect_delivered(1)
        .expect_dropped(0)
        .expect_sent_between(0, 2)
        .expect_delivered_between(1, 1)
        .expect_dropped_between(0, 0);
    EXPECT_EQ(scn.node("A").id(), 0u);
    EXPECT_EQ(scn.network().now().count(), 100'000'000LL);
    EXPECT_FALSE(scn.trace().empty());
    EXPECT_FALSE(scn.dump_trace().empty());
    EXPECT_FALSE(scn.dump_trace_json().empty());
    scn.strict_tags().lenient_tags();

    // Topology shorthands, faults, typed sends, request/response.
    scenario topo("topology tour");
    bool served = false;
    bool responded = false;
    topo.seed(9)
        .nodes("A", "B", "C")
        .mesh({.latency = 1ms})
        .star("A", {.latency = 1ms})
        .chain({.latency = 1ms})
        .link_directed("A", "B", {.latency = 1ms})
        .mesh({"A", "B"}, {.latency = 1ms})
        .send_at(0s, "A", "B", std::vector<std::byte>{std::byte{1}})
        .on_message("B", [](const message&) {})
        .serve<ping, pong>("B",
                           [&](const ping& p) {
                               served = true;
                               return pong{p.text, p.n + 1};
                           })
        .partition_at(10ms, {"A"}, {"B", "C"})
        .heal_at(20ms)
        .crash_at(30ms, "B")
        .restart_at(40ms, "B");
    auto handle = topo.request<ping, pong>("A", "B", ping{"q", 5});
    handle.retry_every(100ms)
        .timeout_after(1s)
        .on_response([&](const pong&) { responded = true; })
        .on_timeout([] {});
    topo.run_until(100ms);
    EXPECT_TRUE(served);
    EXPECT_TRUE(responded);
}

TEST(ApiSurfaceTest, SerializerReflect) {
    // Round-trips through every serializer specialization.
    const auto round_trip = []<typename T>(const T& v) {
        std::vector<std::byte> out;
        serializer<T>::encode(v, out);
        detail::cursor in(out.data(), out.size());
        return serializer<T>::decode(in);
    };
    EXPECT_EQ(round_trip(ping{"hi", 3}).text, "hi");
    EXPECT_EQ(round_trip(std::string{"abc"}), "abc");
    EXPECT_EQ(round_trip(std::vector<int>{1, 2, 3}).size(), 3u);
    EXPECT_EQ(round_trip(std::optional<int>{7}).value(), 7);
    EXPECT_FALSE(round_trip(std::optional<int>{}).has_value());
    EXPECT_EQ(std::get<std::string>(round_trip(std::variant<int, std::string>{"x"})), "x");
    EXPECT_EQ(round_trip(std::pair<int, std::string>{1, "a"}).second, "a");
    EXPECT_EQ(round_trip(std::array<int, 3>{1, 2, 3})[2], 3);

    // reflect: concept, count, tuple, get, for_each_field.
    static_assert(reflect::reflectable<ping>);
    static_assert(reflect::field_count<ping>() == 2);
    EXPECT_EQ(reflect::field_count<ping>(), 2u);
    ping p{"hi", 3};
    EXPECT_EQ(std::get<0>(reflect::to_tuple(p)), "hi");
    EXPECT_EQ(reflect::get<1>(p), 3);
    std::size_t fields = 0;
    reflect::for_each_field(p, [&](const auto&) { ++fields; });
    EXPECT_EQ(fields, 2u);

    // type_name: the primary (typeid name) is non-empty; a stable tag
    // is opt-in per type, so no value is pinned here.
    EXPECT_FALSE(type_name<ping>::value.empty());
}

TEST(ApiSurfaceTest, TraceJson) {
    // trace_event: default fields, equality.
    trace_event e{};
    e.kind = event_kind::sent;
    e.from = 0;
    e.to = 1;
    e.bytes = 3;
    const trace_event expected{.at = {}, .kind = event_kind::sent, .from = 0, .to = 1,
                               .bytes = 3};
    EXPECT_EQ(e, expected);
    EXPECT_NE(e, trace_event{});

    // Every event kind name and every drop reason name.
    EXPECT_STREQ(event_kind_name(event_kind::sent), "sent");
    EXPECT_STREQ(event_kind_name(event_kind::delivered), "delivered");
    EXPECT_STREQ(event_kind_name(event_kind::dropped), "dropped");
    EXPECT_STREQ(event_kind_name(event_kind::duplicated), "duplicated");
    EXPECT_STREQ(event_kind_name(event_kind::reordered), "reordered");
    EXPECT_STREQ(event_kind_name(event_kind::timer_fired), "timer_fired");
    EXPECT_STREQ(event_kind_name(event_kind::node_crashed), "node_crashed");
    EXPECT_STREQ(event_kind_name(event_kind::node_restarted), "node_restarted");
    EXPECT_STREQ(event_kind_name(event_kind::partitioned), "partitioned");
    EXPECT_STREQ(event_kind_name(event_kind::healed), "healed");
    EXPECT_STREQ(event_kind_name(event_kind::request_sent), "request_sent");
    EXPECT_STREQ(event_kind_name(event_kind::response_received), "response_received");
    EXPECT_STREQ(event_kind_name(event_kind::request_timeout), "request_timeout");
    EXPECT_STREQ(drop_reason_name(drop_reason::loss), "loss");
    EXPECT_STREQ(drop_reason_name(drop_reason::partition), "partition");
    EXPECT_STREQ(drop_reason_name(drop_reason::crashed), "crashed");

    // json::value: every alternative, plus dump (both forms).
    using json::array;
    using json::object;
    using json::value;
    EXPECT_EQ(json::dump(value{nullptr}), "null");
    EXPECT_EQ(json::dump(value{true}), "true");
    EXPECT_EQ(json::dump(value{std::int64_t{-5}}), "-5");
    EXPECT_EQ(json::dump(value{std::uint64_t{5}}), "5");
    EXPECT_EQ(json::dump(value{1.5}), "1.5");
    EXPECT_EQ(json::dump(value{std::string{"x"}}), "\"x\"");
    EXPECT_EQ(json::dump(value{array{}}), "[]");
    EXPECT_EQ(json::dump(value{object{}}), "{}");
    std::ostringstream out;
    json::dump(value{std::int64_t{42}}, out);
    EXPECT_EQ(out.str(), "42");

    // json::to_json: every supported type.
    EXPECT_EQ(json::dump(json::to_json(true)), "true");
    EXPECT_EQ(json::dump(json::to_json(42)), "42");
    EXPECT_EQ(json::dump(json::to_json(42u)), "42");
    EXPECT_EQ(json::dump(json::to_json(1.5)), "1.5");
    EXPECT_EQ(json::dump(json::to_json(std::string{"s"})), "\"s\"");
    EXPECT_EQ(json::dump(json::to_json(std::string_view{"s"})), "\"s\"");
    EXPECT_EQ(json::dump(json::to_json(5ms)), "5");  // .count() in its own unit
    EXPECT_EQ(json::dump(json::to_json(std::vector<int>{1, 2})), "[1,2]");
    EXPECT_EQ(json::dump(json::to_json(std::array<int, 2>{1, 2})), "[1,2]");
    EXPECT_EQ(json::dump(json::to_json(std::pair<int, std::string>{1, "a"})), "[1,\"a\"]");
    EXPECT_EQ(json::dump(json::to_json(std::optional<int>{3})), "3");
    EXPECT_EQ(json::dump(json::to_json(std::optional<int>{})), "null");
    EXPECT_EQ(json::dump(json::to_json(std::variant<int, std::string>{"v"})), "\"v\"");
    EXPECT_EQ(json::dump(json::to_json(event_kind::sent)), "\"sent\"");
    EXPECT_EQ(json::dump(json::to_json(drop_reason::loss)), "\"loss\"");
    EXPECT_EQ(json::dump(json::to_json(ping{"hi", 3})), "{\"text\":\"hi\",\"n\":3}");
    EXPECT_EQ(json::dump(json::to_json(pong{"yo", 4})), "{\"text\":\"yo\",\"n\":4}");
    EXPECT_FALSE(json::dump(json::to_json(trace_event{})).empty());
}

TEST(ApiSurfaceTest, Udp) {
    network net(3);
    node& a = net.add_node("A");
    node& b = net.add_node("B");
    net.link(a.id(), b.id(), {.latency = 10ms});

    udp::socket sender(net, a);
    udp::socket receiver(net, b);
    EXPECT_EQ((udp::endpoint{0, 0}), (udp::endpoint{0, 0}));
    EXPECT_NE((udp::endpoint{0, 8080}), (udp::endpoint{0, 8081}));

    sender.bind(0);
    EXPECT_NE(sender.local_endpoint().port, 0);
    receiver.bind(8000);
    EXPECT_EQ(receiver.local_endpoint().port, 8000);
    EXPECT_TRUE(sender.is_open());
    EXPECT_FALSE(sender.is_connected());
    sender.connect(udp::endpoint{b.id(), 8000});
    EXPECT_TRUE(sender.is_connected());

    std::error_code send_ec{1, std::generic_category()};
    std::size_t sent_n = 99;
    std::error_code recv_ec{1, std::generic_category()};
    std::size_t recv_n = 99;
    udp::endpoint recv_from{99, 99};
    std::array<std::byte, 16> buf{};
    receiver.async_receive_from(buf, [&](std::error_code ec, std::size_t n, udp::endpoint from) {
        recv_ec = ec;
        recv_n = n;
        recv_from = from;
    });
    sender.async_send(as_bytes("hi"), [&](std::error_code ec, std::size_t n) {
        send_ec = ec;
        sent_n = n;
    });
    net.run_for(100ms);
    EXPECT_FALSE(send_ec);
    EXPECT_EQ(sent_n, 2u);
    EXPECT_FALSE(recv_ec);
    EXPECT_EQ(recv_n, 2u);
    EXPECT_EQ(as_string(buf.data(), recv_n), "hi");
    EXPECT_EQ(recv_from.node, a.id());

    // async_send_to + async_receive (connected) on a second pair.
    udp::socket s2(net, a);
    udp::socket r2(net, b);
    s2.bind(0);
    r2.bind(8001);
    std::error_code send2_ec{1, std::generic_category()};
    std::size_t send2_n = 99;
    std::array<std::byte, 16> buf2{};
    r2.connect(udp::endpoint{a.id(), s2.local_endpoint().port});
    r2.async_receive(buf2, [&](std::error_code, std::size_t) {});
    s2.async_send_to(as_bytes("yo"), udp::endpoint{b.id(), 8001},
                     [&](std::error_code ec, std::size_t n) {
                         send2_ec = ec;
                         send2_n = n;
                     });
    net.run_for(100ms);
    EXPECT_FALSE(send2_ec);
    EXPECT_EQ(send2_n, 2u);

    // cancel(): a pending receive completes with operation_canceled.
    udp::socket s3(net, a);
    s3.bind(0);
    std::error_code cancel_ec{};
    std::array<std::byte, 16> buf3{};
    s3.async_receive_from(buf3, [&](std::error_code ec, std::size_t, udp::endpoint) {
        cancel_ec = ec;
    });
    s3.cancel();
    net.run_for(10ms);
    EXPECT_EQ(cancel_ec, std::make_error_code(std::errc::operation_canceled));

    // Move construction and move assignment.
    udp::socket moved(std::move(sender));
    EXPECT_TRUE(moved.is_connected());
    udp::socket assigned(net, a);
    assigned = std::move(moved);
    EXPECT_TRUE(assigned.is_connected());
    EXPECT_TRUE(assigned.is_open());
    assigned.close();
    EXPECT_FALSE(assigned.is_open());
}

TEST(ApiSurfaceTest, Tcp) {
    network net(5);
    node& a = net.add_node("A");
    node& b = net.add_node("B");
    net.link(a.id(), b.id(), {.latency = 20ms});

    EXPECT_EQ((tcp::endpoint{0, 0}), (tcp::endpoint{0, 0}));
    EXPECT_NE((tcp::endpoint{0, 9000}), (tcp::endpoint{0, 9001}));

    tcp::acceptor acceptor(net, b);
    acceptor.bind(9000);
    acceptor.listen();
    EXPECT_EQ(acceptor.local_endpoint().port, 9000);
    EXPECT_TRUE(acceptor.is_open());

    tcp::socket accepted(net, b);
    std::error_code accept_ec{1, std::generic_category()};
    acceptor.async_accept([&](tcp::socket s, std::error_code ec) {
        accepted = std::move(s);
        accept_ec = ec;
    });
    tcp::socket client(net, a);
    std::error_code connect_ec{1, std::generic_category()};
    client.async_connect(tcp::endpoint{b.id(), 9000},
                         [&](std::error_code ec) { connect_ec = ec; });
    net.run_until(200ms);
    EXPECT_FALSE(connect_ec);
    EXPECT_FALSE(accept_ec);
    EXPECT_EQ(client.remote_endpoint().port, 9000);
    EXPECT_TRUE(client.is_open());

    // Client writes via the span overload; server reads, writes back via
    // the string_view overload; client reads.
    std::array<std::byte, 16> server_buf{};
    std::array<std::byte, 16> client_buf{};
    std::error_code server_read_ec{1, std::generic_category()};
    std::size_t server_read_n = 99;
    std::error_code client_read_ec{1, std::generic_category()};
    std::size_t client_read_n = 99;
    accepted.async_read_some(server_buf, [&](std::error_code ec, std::size_t n) {
        server_read_ec = ec;
        server_read_n = n;
        accepted.async_write("pong", [](std::error_code, std::size_t) {});
    });
    client.async_write(as_bytes("ping"), [](std::error_code, std::size_t) {});
    client.async_read_some(client_buf, [&](std::error_code ec, std::size_t n) {
        client_read_ec = ec;
        client_read_n = n;
    });
    net.run_until(1s);
    EXPECT_FALSE(server_read_ec);
    EXPECT_EQ(server_read_n, 4u);
    EXPECT_EQ(as_string(server_buf.data(), server_read_n), "ping");
    EXPECT_FALSE(client_read_ec);
    EXPECT_EQ(client_read_n, 4u);
    EXPECT_EQ(as_string(client_buf.data(), client_read_n), "pong");

    // Move construction and move assignment; close.
    tcp::socket moved(std::move(client));
    EXPECT_TRUE(moved.is_open());
    tcp::socket assigned(net, a);
    assigned = std::move(moved);
    EXPECT_TRUE(assigned.is_open());
    assigned.close();
    EXPECT_FALSE(assigned.is_open());
    tcp::acceptor moved_acc(std::move(acceptor));
    EXPECT_TRUE(moved_acc.is_open());
    moved_acc.close();
    EXPECT_FALSE(moved_acc.is_open());
}

TEST(ApiSurfaceTest, Errors) {
    // Each named type constructs, and catches under its documented base.
    try {
        throw assertion_failed("expected 1, got 0");
    } catch (const std::runtime_error& e) {
        EXPECT_STREQ(e.what(), "expected 1, got 0");
    }
    try {
        throw decode_error("tag mismatch");
    } catch (const std::invalid_argument& e) {
        EXPECT_STREQ(e.what(), "tag mismatch");
    }
    try {
        throw unhandled_message("no handler for tag 7");
    } catch (const std::runtime_error& e) {
        EXPECT_STREQ(e.what(), "no handler for tag 7");
    }
    static_assert(std::is_base_of_v<std::invalid_argument, decode_error>);
    static_assert(std::is_base_of_v<std::runtime_error, assertion_failed>);
    static_assert(std::is_base_of_v<std::runtime_error, unhandled_message>);
}

}  // namespace
