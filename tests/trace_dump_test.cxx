// Event tracing: the dump (golden format, stream/string equivalence,
// per-event formatting), determinism and transparency.

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <functional>
#include <sstream>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "simulnetx/network.hxx"

namespace {

using namespace std::chrono_literals;
using simulnetx::event_kind;
using simulnetx::trace_event;

// The canonical retry scenario (examples/retry_under_loss.cxx): A pings
// B every 100ms until the ack arrives; 30% loss both ways; partitioned
// 200ms-1s.
struct retry_outcome {
    bool acked{};
    int attempts{};
    long long acked_at{-1};
    std::size_t sent{};
    std::size_t delivered{};
    std::size_t dropped{};
    std::string delivery_log;  // 'P' per ping B receives, 'A' per ack A receives
    std::vector<trace_event> trace;
    std::string dump;
};

// Runs the retry scenario on a fresh network and hands the live network
// (non-movable, so it cannot be returned) plus the outcome to fn.
template <typename F>
retry_outcome run_retry_with(bool tracing, F&& fn) {
    simulnetx::network net(42);
    net.set_tracing(tracing);
    auto& a = net.add_node("A");
    auto& b = net.add_node("B");
    net.link(a.id(), b.id(), {.latency = 20ms, .loss_rate = 0.3});

    retry_outcome out;
    b.on_message([&](const simulnetx::message& m) {
        if (m.payload.size() == 4) {  // "ping"
            out.delivery_log += 'P';
            b.send(a.id(), "ack");
        }
    });
    a.on_message([&](const simulnetx::message& m) {
        if (m.payload.size() == 3) {  // "ack"
            out.delivery_log += 'A';
            out.acked = true;
            out.acked_at = net.now().count();
        }
    });

    std::function<void()> retry;
    retry = [&] {
        if (out.acked) {
            return;
        }
        ++out.attempts;
        a.send(b.id(), "ping");
        a.after(100ms, retry);
    };
    a.after(0ns, retry);
    a.after(200ms, [&] { net.partition({a.id()}, {b.id()}); });
    a.after(1s, [&] { net.heal(); });

    net.run_for(5s);
    out.sent = static_cast<std::size_t>(net.sent_count());
    out.delivered = static_cast<std::size_t>(net.delivered_count());
    out.dropped = static_cast<std::size_t>(net.dropped_count());
    out.trace = net.trace();
    out.dump = net.dump_trace();
    fn(net, out);
    return out;
}

retry_outcome run_retry(bool tracing) {
    return run_retry_with(tracing, [](simulnetx::network&, const retry_outcome&) {});
}

TEST(TraceDumpTest, GoldenTinyExample) {
    // Determined by construction: A -> B, 50ms latency, no loss, one
    // send at t=0. The exact dump, byte for byte.
    simulnetx::network net(42);
    auto& a = net.add_node("A");
    auto& b = net.add_node("B");
    net.link(a.id(), b.id(), {.latency = 50ms});
    a.send(b.id(), "hi");
    net.run_for(1s);

    EXPECT_EQ(net.dump_trace(),
              "trace: 2 events (seed 42)\n"
              "t=0ns A -> B sent 2 bytes\n"
              "t=50000000ns A -> B delivered 2 bytes\n");
}

TEST(TraceDumpTest, EmptyTraceDumpsHeaderOnly) {
    simulnetx::network net(42);
    net.add_node("A");
    EXPECT_EQ(net.dump_trace(), "trace: 0 events (seed 42)\n");
}

TEST(TraceDumpTest, StreamDumpEqualsStringDump) {
    run_retry_with(true, [](simulnetx::network& net, const retry_outcome& out) {
        ASSERT_EQ(out.trace.size(), 52U);
        std::ostringstream oss;
        net.dump_trace(oss);
        EXPECT_EQ(oss.str(), net.dump_trace());
        EXPECT_EQ(oss.str(), out.dump);
    });
}

TEST(TraceDumpTest, EveryLineEqualsFormatEvent) {
    run_retry_with(true, [](simulnetx::network& net, const retry_outcome& out) {
        ASSERT_EQ(out.trace.size(), 52U);

        // Split the dump into lines; the first is the header, the rest
        // must each equal format_event of the corresponding event.
        std::vector<std::string> lines;
        std::istringstream in(out.dump);
        std::string line;
        while (std::getline(in, line)) {
            lines.push_back(line);
        }
        ASSERT_EQ(lines.size(), 53U);
        EXPECT_EQ(lines[0], "trace: 52 events (seed 42)");
        for (std::size_t i = 0; i < out.trace.size(); ++i) {
            EXPECT_EQ(net.format_event(out.trace[i]), lines[i + 1]) << "event " << i;
        }
    });
}

TEST(TraceDumpTest, FilteredDumpMatchesDroppedLines) {
    run_retry_with(true, [](simulnetx::network& net, const retry_outcome& out) {
        ASSERT_EQ(out.trace.size(), 52U);

        // A drops-only dump built with format_event.
        std::string filtered;
        for (const trace_event& e : net.trace()) {
            if (e.kind == event_kind::dropped) {
                filtered += net.format_event(e);
                filtered += '\n';
            }
        }

        // The same lines, picked out of the full dump.
        std::string expected;
        std::istringstream in(out.dump);
        std::string line;
        while (std::getline(in, line)) {
            if (line.find(" dropped (") != std::string::npos) {
                expected += line;
                expected += '\n';
            }
        }
        EXPECT_EQ(filtered, expected);
        EXPECT_EQ(std::count(filtered.begin(), filtered.end(), '\n'), 12);
    });
}

TEST(TraceDumpTest, UnknownNodeIdFormatsAsNodeHash) {
    simulnetx::network net(42);
    net.add_node("A");
    const trace_event bogus{.kind = event_kind::sent, .from = 999, .to = 0, .bytes = 1};
    EXPECT_EQ(net.format_event(bogus), "t=0ns node#999 -> A sent 1 bytes");
}

TEST(TraceDumpTest, DeterminismTraceAndDump) {
    const retry_outcome first = run_retry(true);
    const retry_outcome second = run_retry(true);
    EXPECT_EQ(first.trace, second.trace);
    EXPECT_EQ(first.dump, second.dump);
}

TEST(TraceDumpTest, TransparencyTracingOff) {
    const retry_outcome traced = run_retry(true);
    const retry_outcome untraced = run_retry(false);
    EXPECT_EQ(untraced.sent, traced.sent);
    EXPECT_EQ(untraced.delivered, traced.delivered);
    EXPECT_EQ(untraced.dropped, traced.dropped);
    EXPECT_EQ(untraced.attempts, traced.attempts);
    EXPECT_EQ(untraced.acked, traced.acked);
    EXPECT_EQ(untraced.acked_at, traced.acked_at);
    EXPECT_EQ(untraced.delivery_log, traced.delivery_log);
    EXPECT_TRUE(untraced.trace.empty());
    EXPECT_EQ(untraced.dump, "trace: 0 events (seed 42)\n");
}

TEST(TraceDumpTest, EventCountExpectations) {
    simulnetx::network net(42);
    auto& a = net.add_node("A");
    auto& b = net.add_node("B");
    net.link(a.id(), b.id(), {.latency = 50ms, .loss_rate = 1.0});
    a.send(b.id(), "x");
    net.run_for(1s);

    net.event_count(event_kind::sent).equal(1);
    net.event_count(event_kind::dropped).equal(1);
    net.event_count(event_kind::delivered).equal(0);
    net.event_count(event_kind::dropped).between(1, 2);

    try {
        net.event_count(event_kind::dropped).equal(5);
        FAIL() << "expected assertion_failed";
    } catch (const simulnetx::assertion_failed& e) {
        const std::string msg = e.what();
        EXPECT_NE(msg.find("event_count(dropped)"), std::string::npos);
        EXPECT_NE(msg.find("expected event_count(dropped) == 5, got 1"),
                  std::string::npos);
        EXPECT_NE(msg.find("seed 42"), std::string::npos);
    }
}

}  // namespace
