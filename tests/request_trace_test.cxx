// Event tracing: typed-layer request/response events (request_sent,
// response_received, request_timeout) on the same timeline as the
// transport events.

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "simulnetx/network.hxx"

namespace {

using namespace std::chrono_literals;
using simulnetx::event_kind;
using simulnetx::trace_event;

struct hello {
    std::string name;
};
struct world {
    std::string greeting;
};

std::size_t count_kind(const std::vector<trace_event>& trace, event_kind kind) {
    return static_cast<std::size_t>(std::count_if(
        trace.begin(), trace.end(),
        [kind](const trace_event& e) { return e.kind == kind; }));
}

std::vector<trace_event> of_kind(const std::vector<trace_event>& trace, event_kind kind) {
    std::vector<trace_event> out;
    for (const trace_event& e : trace) {
        if (e.kind == kind) {
            out.push_back(e);
        }
    }
    return out;
}

TEST(RequestTraceTest, RetriesCarryAttemptNumbersAndResponseMatchesId) {
    // Same setup as the spike's typed demo (seed 42, 50ms latency, 30%
    // loss): 5 attempts, the response arrives on the 5th, at 4.1s.
    simulnetx::network net(42);
    auto& client = net.add_node("client");
    auto& server = net.add_node("server");
    net.link(client.id(), server.id(), {.latency = 50ms, .loss_rate = 0.3});
    int served = 0;
    server.serve<hello, world>([&](const hello& h) {
        ++served;
        return world{"hello, " + h.name};
    });
    std::string answer;
    client.request<hello, world>(server.id(), hello{"reim"})
        .retry_every(1s)
        .timeout_after(10s)
        .on_response([&](const world& w) { answer = w.greeting; });
    net.run_until(10s);

    EXPECT_EQ(answer, "hello, reim");
    EXPECT_EQ(served, 4);

    const std::vector<trace_event>& trace = net.trace();
    ASSERT_EQ(trace.size(), 30U);
    const std::vector<trace_event> sent = of_kind(trace, event_kind::request_sent);
    ASSERT_EQ(sent.size(), 5U);
    for (std::size_t i = 0; i < sent.size(); ++i) {
        EXPECT_EQ(sent[i].request_id, 0U);
        EXPECT_EQ(sent[i].attempt, static_cast<unsigned>(i + 1));
        EXPECT_EQ(sent[i].from, client.id());
        EXPECT_EQ(sent[i].to, server.id());
        EXPECT_EQ(sent[i].at, std::chrono::nanoseconds(static_cast<long long>(i) * 1000000000LL));
    }
    const std::vector<trace_event> received =
        of_kind(trace, event_kind::response_received);
    ASSERT_EQ(received.size(), 1U);
    EXPECT_EQ(received[0].request_id, 0U);
    EXPECT_EQ(received[0].from, server.id());
    EXPECT_EQ(received[0].to, client.id());
    EXPECT_EQ(received[0].at, 4100ms);
    EXPECT_EQ(count_kind(trace, event_kind::request_timeout), 0U);
}

TEST(RequestTraceTest, TimeoutRecordedOnce) {
    simulnetx::network net(42);
    auto& client = net.add_node("client");
    auto& server = net.add_node("server");
    net.link(client.id(), server.id(), {.latency = 50ms});
    // No serve handler: the server never replies.
    bool timed_out = false;
    client.request<hello, world>(server.id(), hello{"reim"})
        .timeout_after(5s)
        .on_timeout([&] { timed_out = true; });
    net.run_until(10s);

    EXPECT_TRUE(timed_out);
    const std::vector<trace_event>& trace = net.trace();
    const std::vector<trace_event> timeouts =
        of_kind(trace, event_kind::request_timeout);
    ASSERT_EQ(timeouts.size(), 1U);
    EXPECT_EQ(timeouts[0].request_id, 0U);
    EXPECT_EQ(timeouts[0].node, client.id());
    EXPECT_EQ(timeouts[0].at, 5s);
    EXPECT_EQ(count_kind(trace, event_kind::response_received), 0U);
}

TEST(RequestTraceTest, LateResponseRecordsNothing) {
    // The reply arrives after the timeout: the engine ignores it, and the
    // trace records no response_received for it.
    simulnetx::network net(42);
    auto& client = net.add_node("client");
    auto& server = net.add_node("server");
    net.link(client.id(), server.id(), {.latency = 6s});
    server.serve<hello, world>(
        [](const hello& h) { return world{"hello, " + h.name}; });
    bool got_response = false;
    client.request<hello, world>(server.id(), hello{"reim"})
        .timeout_after(5s)
        .on_response([&](const world&) { got_response = true; });
    net.run_until(20s);

    EXPECT_FALSE(got_response);
    const std::vector<trace_event>& trace = net.trace();
    EXPECT_EQ(count_kind(trace, event_kind::request_sent), 1U);
    EXPECT_EQ(count_kind(trace, event_kind::request_timeout), 1U);
    EXPECT_EQ(count_kind(trace, event_kind::response_received), 0U);
}

TEST(RequestTraceTest, CrashCancelsRequestSilently) {
    // The client crashes mid-retry: no further attempts, no timeout event.
    simulnetx::network net(42);
    auto& client = net.add_node("client");
    auto& server = net.add_node("server");
    net.link(client.id(), server.id(), {.latency = 50ms, .loss_rate = 1.0});
    client.request<hello, world>(server.id(), hello{"reim"})
        .retry_every(1s)
        .timeout_after(10s);
    net.after(2500ms, [&] { net.crash(client.id()); });
    net.run_until(15s);

    const std::vector<trace_event>& trace = net.trace();
    // Attempts at 0s, 1s, 2s; the 3s retry never fires (node dead).
    const std::vector<trace_event> sent = of_kind(trace, event_kind::request_sent);
    ASSERT_EQ(sent.size(), 3U);
    EXPECT_EQ(sent[2].attempt, 3U);
    EXPECT_EQ(sent[2].at, 2s);
    EXPECT_EQ(count_kind(trace, event_kind::request_timeout), 0U);
    EXPECT_EQ(count_kind(trace, event_kind::node_crashed), 1U);
}

TEST(RequestTraceTest, TwoRequestsHaveDistinctIds) {
    simulnetx::network net(42);
    auto& client = net.add_node("client");
    auto& server = net.add_node("server");
    net.link(client.id(), server.id(), {.latency = 50ms});
    server.serve<hello, world>(
        [](const hello& h) { return world{"hello, " + h.name}; });
    int responses = 0;
    client.request<hello, world>(server.id(), hello{"one"})
        .on_response([&](const world&) { ++responses; });
    client.request<hello, world>(server.id(), hello{"two"})
        .on_response([&](const world&) { ++responses; });
    net.run_until(5s);

    EXPECT_EQ(responses, 2);
    const std::vector<trace_event>& trace = net.trace();
    const std::vector<trace_event> sent = of_kind(trace, event_kind::request_sent);
    ASSERT_EQ(sent.size(), 2U);
    EXPECT_EQ(sent[0].request_id, 0U);
    EXPECT_EQ(sent[1].request_id, 1U);
    const std::vector<trace_event> received =
        of_kind(trace, event_kind::response_received);
    ASSERT_EQ(received.size(), 2U);
    EXPECT_EQ(received[0].request_id, 0U);
    EXPECT_EQ(received[1].request_id, 1U);
}

TEST(RequestTraceTest, DroppedSendOrdersSentDroppedRequestSent) {
    // request_sent is recorded after the send returns, so a send-time
    // drop appears between the transport sent and the request event.
    simulnetx::network net(42);
    auto& client = net.add_node("client");
    auto& server = net.add_node("server");
    net.link(client.id(), server.id(), {.latency = 50ms, .loss_rate = 1.0});
    client.request<hello, world>(server.id(), hello{"reim"});
    net.run_for(1s);

    const std::vector<trace_event>& trace = net.trace();
    ASSERT_EQ(trace.size(), 3U);
    EXPECT_EQ(trace[0].kind, event_kind::sent);
    EXPECT_EQ(trace[1].kind, event_kind::dropped);
    EXPECT_EQ(trace[1].reason, simulnetx::drop_reason::loss);
    EXPECT_EQ(trace[2].kind, event_kind::request_sent);
    EXPECT_EQ(trace[2].request_id, 0U);
    EXPECT_EQ(trace[2].attempt, 1U);
    EXPECT_EQ(trace[0].at, 0ns);
    EXPECT_EQ(trace[1].at, 0ns);
    EXPECT_EQ(trace[2].at, 0ns);
}

}  // namespace
