// Event tracing: transport recording (sent/delivered/dropped/duplicated/
// reordered), drop reasons, type tags, and the tracing toggle.

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "simulnetx/network.hxx"
#include "simulnetx/serializer.hxx"

namespace {

using namespace std::chrono_literals;
using simulnetx::drop_reason;
using simulnetx::event_kind;
using simulnetx::trace_event;

struct ping {
    std::string text;
};

std::size_t count_kind(const std::vector<trace_event>& trace, event_kind kind) {
    return static_cast<std::size_t>(std::count_if(
        trace.begin(), trace.end(),
        [kind](const trace_event& e) { return e.kind == kind; }));
}

std::size_t count_drops_with(const std::vector<trace_event>& trace, drop_reason reason) {
    return static_cast<std::size_t>(std::count_if(
        trace.begin(), trace.end(), [reason](const trace_event& e) {
            return e.kind == event_kind::dropped && e.reason == reason;
        }));
}

TEST(TraceTest, EmptyTraceOnFreshNetwork) {
    simulnetx::network net(42);
    EXPECT_TRUE(net.trace().empty());
    EXPECT_TRUE(net.tracing());
}

TEST(TraceTest, SingleSendRecordsSentAndDelivered) {
    simulnetx::network net(42);
    auto& a = net.add_node("A");
    auto& b = net.add_node("B");
    net.link(a.id(), b.id(), {.latency = 50ms});
    a.send(b.id(), "hi");
    net.run_for(1s);

    const std::vector<trace_event>& trace = net.trace();
    ASSERT_EQ(trace.size(), 2U);
    EXPECT_EQ(trace[0], (trace_event{.at = 0ns,
                                     .kind = event_kind::sent,
                                     .from = a.id(),
                                     .to = b.id(),
                                     .bytes = 2,
                                     .tag = 0}));
    EXPECT_EQ(trace[1], (trace_event{.at = 50ms,
                                     .kind = event_kind::delivered,
                                     .from = a.id(),
                                     .to = b.id(),
                                     .bytes = 2,
                                     .tag = 0}));
}

TEST(TraceTest, CountsMatchStats) {
    // Golden values from GoldenTest.LossSeed7: 200 sent, 94 dropped, 106
    // delivered. The trace must agree with the counters exactly.
    simulnetx::network net(7);
    auto& s = net.add_node("S");
    auto& r = net.add_node("R");
    net.link(s.id(), r.id(), {.loss_rate = 0.5});
    for (int i = 0; i < 200; ++i) {
        s.send(r.id(), "p");
    }
    net.run_for(1s);

    const std::vector<trace_event>& trace = net.trace();
    EXPECT_EQ(count_kind(trace, event_kind::sent), 200U);
    EXPECT_EQ(count_kind(trace, event_kind::delivered), 106U);
    EXPECT_EQ(count_kind(trace, event_kind::dropped), 94U);
    EXPECT_EQ(count_kind(trace, event_kind::sent),
              static_cast<std::size_t>(net.sent_count()));
    EXPECT_EQ(count_kind(trace, event_kind::delivered),
              static_cast<std::size_t>(net.delivered_count()));
    EXPECT_EQ(count_kind(trace, event_kind::dropped),
              static_cast<std::size_t>(net.dropped_count()));
}

TEST(TraceTest, LossDropsCarryLossReason) {
    simulnetx::network net(7);
    auto& s = net.add_node("S");
    auto& r = net.add_node("R");
    net.link(s.id(), r.id(), {.loss_rate = 0.5});
    for (int i = 0; i < 200; ++i) {
        s.send(r.id(), "p");
    }
    net.run_for(1s);

    const std::vector<trace_event>& trace = net.trace();
    EXPECT_EQ(count_drops_with(trace, drop_reason::loss), 94U);
    EXPECT_EQ(count_drops_with(trace, drop_reason::partition), 0U);
    EXPECT_EQ(count_drops_with(trace, drop_reason::crashed), 0U);
}

TEST(TraceTest, PartitionDropsAtSendTime) {
    simulnetx::network net(42);
    auto& a = net.add_node("A");
    auto& b = net.add_node("B");
    net.link(a.id(), b.id(), {.latency = 10ms});
    net.partition({a.id()}, {b.id()});
    a.send(b.id(), "ping");
    net.run_for(1s);

    const std::vector<trace_event>& trace = net.trace();
    ASSERT_EQ(trace.size(), 2U);
    EXPECT_EQ(trace[0].kind, event_kind::sent);
    EXPECT_EQ(trace[1].kind, event_kind::dropped);
    EXPECT_EQ(trace[1].reason, drop_reason::partition);
    EXPECT_EQ(trace[1].at, 0ns);
    EXPECT_EQ(trace[1].from, a.id());
    EXPECT_EQ(trace[1].to, b.id());
    EXPECT_EQ(trace[1].bytes, 4U);
}

TEST(TraceTest, PartitionDropsInFlightAtDeliveryTime) {
    simulnetx::network net(42);
    auto& a = net.add_node("A");
    auto& b = net.add_node("B");
    net.link(a.id(), b.id(), {.latency = 100ms});
    a.send(b.id(), "ping");
    net.after(50ms, [&] { net.partition({a.id()}, {b.id()}); });
    net.run_for(1s);

    const std::vector<trace_event>& trace = net.trace();
    ASSERT_EQ(trace.size(), 2U);
    EXPECT_EQ(trace[0].kind, event_kind::sent);
    EXPECT_EQ(trace[0].at, 0ns);
    EXPECT_EQ(trace[1].kind, event_kind::dropped);
    EXPECT_EQ(trace[1].reason, drop_reason::partition);
    EXPECT_EQ(trace[1].at, 100ms);
}

TEST(TraceTest, CrashedDestinationDropsAtSendTime) {
    simulnetx::network net(42);
    auto& a = net.add_node("A");
    auto& b = net.add_node("B");
    net.link(a.id(), b.id(), {.latency = 10ms});
    net.crash(b.id());
    a.send(b.id(), "ping");
    net.run_for(1s);

    const std::vector<trace_event>& trace = net.trace();
    ASSERT_EQ(trace.size(), 2U);
    EXPECT_EQ(trace[1].kind, event_kind::dropped);
    EXPECT_EQ(trace[1].reason, drop_reason::crashed);
    EXPECT_EQ(trace[1].at, 0ns);
}

TEST(TraceTest, CrashedDestinationDropsInFlightAtDeliveryTime) {
    simulnetx::network net(42);
    auto& a = net.add_node("A");
    auto& b = net.add_node("B");
    net.link(a.id(), b.id(), {.latency = 100ms});
    a.send(b.id(), "ping");
    net.after(50ms, [&] { net.crash(b.id()); });
    net.run_for(1s);

    const std::vector<trace_event>& trace = net.trace();
    ASSERT_EQ(trace.size(), 2U);
    EXPECT_EQ(trace[1].kind, event_kind::dropped);
    EXPECT_EQ(trace[1].reason, drop_reason::crashed);
    EXPECT_EQ(trace[1].at, 100ms);
}

TEST(TraceTest, SelfSendRecordsSentAndDelivered) {
    simulnetx::network net(42);
    auto& a = net.add_node("A");
    a.send(a.id(), "me");
    net.run_for(1s);

    const std::vector<trace_event>& trace = net.trace();
    ASSERT_EQ(trace.size(), 2U);
    EXPECT_EQ(trace[0].kind, event_kind::sent);
    EXPECT_EQ(trace[0].from, a.id());
    EXPECT_EQ(trace[0].to, a.id());
    EXPECT_EQ(trace[1].kind, event_kind::delivered);
    EXPECT_EQ(trace[1].at, 0ns);
}

TEST(TraceTest, DupRecordsDuplicatedAndSecondDelivery) {
    // Golden values from GoldenTest.DupSeed11: 200 sent, 294 delivered.
    // The 94 extra deliveries are dup copies: each is a `duplicated`
    // event at send time plus a second `delivered` event 1ns later.
    // A dup copy is NOT a `sent` event (send_from counts once per call).
    simulnetx::network net(11);
    auto& s = net.add_node("S");
    auto& r = net.add_node("R");
    net.link(s.id(), r.id(), {.dup_rate = 0.5});
    for (int i = 0; i < 200; ++i) {
        s.send(r.id(), "p");
    }
    net.run_for(1s);

    const std::vector<trace_event>& trace = net.trace();
    EXPECT_EQ(count_kind(trace, event_kind::sent), 200U);
    EXPECT_EQ(count_kind(trace, event_kind::duplicated), 94U);
    EXPECT_EQ(count_kind(trace, event_kind::delivered), 294U);

    // Latency is 0, so every original is delivered at t=0 and every dup
    // copy exactly 1ns later: 200 deliveries at 0ns, 94 at 1ns.
    const std::size_t at_zero = static_cast<std::size_t>(std::count_if(
        trace.begin(), trace.end(), [](const trace_event& e) {
            return e.kind == event_kind::delivered && e.at == 0ns;
        }));
    const std::size_t at_one_ns = static_cast<std::size_t>(std::count_if(
        trace.begin(), trace.end(), [](const trace_event& e) {
            return e.kind == event_kind::delivered && e.at == 1ns;
        }));
    EXPECT_EQ(at_zero, 200U);
    EXPECT_EQ(at_one_ns, 94U);
}

TEST(TraceTest, ReorderRecordsReordered) {
    simulnetx::network net(13);
    auto& s = net.add_node("S");
    auto& r = net.add_node("R");
    net.link(s.id(), r.id(), {.jitter = 10ms, .reorder_rate = 0.5});
    for (int i = 0; i < 50; ++i) {
        s.send(r.id(), std::vector<std::byte>{static_cast<std::byte>(i)});
    }
    net.run_for(1s);

    const std::vector<trace_event>& trace = net.trace();
    EXPECT_EQ(count_kind(trace, event_kind::sent), 50U);
    EXPECT_EQ(count_kind(trace, event_kind::delivered), 50U);
    const std::size_t reordered = count_kind(trace, event_kind::reordered);
    EXPECT_GT(reordered, 0U);
    EXPECT_LT(reordered, 50U);
}

TEST(TraceTest, TypedSendRecordsTag) {
    simulnetx::network net(42);
    auto& a = net.add_node("A");
    auto& b = net.add_node("B");
    net.link(a.id(), b.id(), {.latency = 10ms});
    b.on<ping>([](const ping&) {});
    // Catch-all so the raw message does not trip strict-mode dispatch.
    b.on_message([](const simulnetx::message&) {});
    a.send<ping>(b.id(), ping{"hello"});
    a.send(b.id(), "raw");
    net.run_for(1s);

    const std::vector<trace_event>& trace = net.trace();
    ASSERT_EQ(trace.size(), 4U);
    const std::uint64_t tag = simulnetx::detail::type_tag<ping>();
    EXPECT_NE(tag, 0U);
    // Typed send: tag on both the sent and the delivered event.
    EXPECT_EQ(trace[0].kind, event_kind::sent);
    EXPECT_EQ(trace[0].tag, tag);
    EXPECT_EQ(trace[2].kind, event_kind::delivered);
    EXPECT_EQ(trace[2].tag, tag);
    // Raw send: tag 0 on both events.
    EXPECT_EQ(trace[1].kind, event_kind::sent);
    EXPECT_EQ(trace[1].tag, 0U);
    EXPECT_EQ(trace[3].kind, event_kind::delivered);
    EXPECT_EQ(trace[3].tag, 0U);
}

TEST(TraceTest, TracingToggleAndClear) {
    simulnetx::network net(42);
    auto& a = net.add_node("A");
    auto& b = net.add_node("B");
    net.link(a.id(), b.id(), {.latency = 10ms});

    a.send(b.id(), "one");
    net.run_for(100ms);
    EXPECT_EQ(net.trace().size(), 2U);

    // Disabling stops recording but keeps what is there.
    net.set_tracing(false);
    EXPECT_FALSE(net.tracing());
    a.send(b.id(), "two");
    net.run_for(100ms);
    EXPECT_EQ(net.trace().size(), 2U);
    EXPECT_EQ(static_cast<std::size_t>(net.sent_count()), 2U);

    // Re-enabling resumes recording.
    net.set_tracing(true);
    a.send(b.id(), "three");
    net.run_for(100ms);
    EXPECT_EQ(net.trace().size(), 4U);

    // Clearing empties the trace; recording continues.
    net.clear_trace();
    EXPECT_TRUE(net.trace().empty());
    a.send(b.id(), "four");
    net.run_for(100ms);
    EXPECT_EQ(net.trace().size(), 2U);
}

TEST(TraceTest, TraceSurvivesException) {
    // Strict mode throws unhandled_message for a message no handler
    // consumes, but the delivered event is recorded before the throw.
    simulnetx::network net(42);
    auto& a = net.add_node("A");
    auto& b = net.add_node("B");
    net.link(a.id(), b.id(), {.latency = 10ms});
    b.on<ping>([](const ping&) {});
    a.send(b.id(), "raw");
    EXPECT_THROW(net.run_for(1s), simulnetx::unhandled_message);

    const std::vector<trace_event>& trace = net.trace();
    ASSERT_EQ(trace.size(), 2U);
    EXPECT_EQ(trace[0].kind, event_kind::sent);
    EXPECT_EQ(trace[1].kind, event_kind::delivered);
}

}  // namespace
