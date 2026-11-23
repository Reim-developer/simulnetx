#include <chrono>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "simulnetx/errors.hxx"
#include "simulnetx/network.hxx"
#include "simulnetx/scenario.hxx"

namespace {

using namespace std::chrono_literals;
using simulnetx::decode_error;
using simulnetx::network;
using simulnetx::scenario;

struct hello {
    std::string name;
    bool operator==(const hello&) const = default;
};

struct world {
    std::string text;
    bool operator==(const world&) const = default;
};

struct other {
    int code;
    bool operator==(const other&) const = default;
};

TEST(RequestTest, BasicRequestResponse) {
    network net{42};
    auto& a = net.add_node("A");
    auto& b = net.add_node("B");
    net.link(a.id(), b.id(), {.latency = 100ms});

    b.serve<hello, world>([](const hello& h) { return world{"hello, " + h.name}; });

    std::vector<world> responses;
    a.request<hello, world>(b.id(), hello{"reim"})
        .on_response([&responses](const world& w) { responses.push_back(w); });
    net.run_until(1s);

    EXPECT_EQ(responses, (std::vector<world>{{"hello, reim"}}));
    EXPECT_EQ(net.sent_count(), 2u);
    EXPECT_EQ(net.delivered_count(), 2u);
}

TEST(RequestTest, RetryAcrossPartitionHeal) {
    network net{42};
    auto& a = net.add_node("A");
    auto& b = net.add_node("B");
    net.link(a.id(), b.id(), {});

    b.serve<hello, world>([](const hello& h) { return world{"hello, " + h.name}; });
    net.partition({a.id()}, {b.id()});
    net.schedule_at(3s, [&net] { net.heal(); });

    int responses = 0;
    a.request<hello, world>(b.id(), hello{"reim"})
        .retry_every(1s)
        .timeout_after(10s)
        .on_response([&responses](const world&) { ++responses; });
    net.run_until(10s);

    // The request at t=0 and the retries at 1s and 2s were dropped by the
    // partition; the retry at 3s went through after the heal (the heal
    // timer was scheduled first, so it fires before the 3s retry).
    EXPECT_EQ(responses, 1);
    EXPECT_EQ(net.sent_count(), 5u);
    EXPECT_EQ(net.dropped_count(), 3u);
    EXPECT_EQ(net.delivered_count(), 2u);
}

TEST(RequestTest, TimeoutFiresWhenServerIsSilent) {
    network net{42};
    auto& a = net.add_node("A");
    auto& b = net.add_node("B");
    net.link(a.id(), b.id(), {});

    // B serves nothing: the request goes unanswered.
    int responses = 0;
    int timeouts = 0;
    std::chrono::nanoseconds timeout_at{0};
    a.request<hello, world>(b.id(), hello{"reim"})
        .timeout_after(2s)
        .on_response([&responses](const world&) { ++responses; })
        .on_timeout([&] {
            ++timeouts;
            timeout_at = net.now();
        });
    net.run_until(5s);

    EXPECT_EQ(responses, 0);
    EXPECT_EQ(timeouts, 1);
    EXPECT_EQ(timeout_at, 2s);
}

TEST(RequestTest, LateResponseIsIgnored) {
    network net{42};
    auto& a = net.add_node("A");
    auto& b = net.add_node("B");
    net.link(a.id(), b.id(), {.latency = 5s});

    b.serve<hello, world>([](const hello&) { return world{"too late"}; });

    int responses = 0;
    int timeouts = 0;
    a.request<hello, world>(b.id(), hello{"reim"})
        .timeout_after(2s)
        .on_response([&responses](const world&) { ++responses; })
        .on_timeout([&timeouts] { ++timeouts; });
    // The response arrives at t=10s, long after the 2s timeout.
    net.run_until(15s);

    EXPECT_EQ(timeouts, 1);
    EXPECT_EQ(responses, 0);
}

TEST(RequestTest, DuplicateResponseDeliversAtMostOnce) {
    network net{42};
    auto& a = net.add_node("A");
    auto& b = net.add_node("B");
    net.link(a.id(), b.id(), {.dup_rate = 1.0});

    b.serve<hello, world>([](const hello&) { return world{"once"}; });

    int responses = 0;
    a.request<hello, world>(b.id(), hello{"reim"})
        .on_response([&responses](const world&) { ++responses; });
    net.run_until(1s);

    // The reply was duplicated (original + 1ns-later copy), but the first
    // response completed the request, so the callback fired exactly once.
    // Note the request itself was duplicated too, so the server served
    // twice and sent two replies (each duplicated): retries and dups are
    // idempotent from the caller's view, not the server's - the server
    // cannot tell a retry from a duplicate.
    EXPECT_EQ(responses, 1);
    EXPECT_EQ(net.sent_count(), 3u);
    EXPECT_EQ(net.delivered_count(), 6u);
}

TEST(RequestTest, CrashCancelsPendingRequestsSilently) {
    network net{42};
    auto& a = net.add_node("A");
    auto& b = net.add_node("B");
    net.link(a.id(), b.id(), {.latency = 5s});

    b.serve<hello, world>([](const hello&) { return world{"nobody home"}; });

    int responses = 0;
    int timeouts = 0;
    a.request<hello, world>(b.id(), hello{"reim"})
        .retry_every(1s)
        .timeout_after(10s)
        .on_response([&responses](const world&) { ++responses; })
        .on_timeout([&timeouts] { ++timeouts; });

    // Crash before the first retry (1s), restart well before the reply
    // arrives (10s): the pending request is gone, the retry/timeout
    // timers were cancelled by the crash, and the late reply is ignored.
    net.schedule_at(500ms, [&net, id = a.id()] { net.crash(id); });
    net.schedule_at(3s, [&net, id = a.id()] { net.restart(id); });
    net.run_until(15s);

    EXPECT_EQ(responses, 0);
    EXPECT_EQ(timeouts, 0);
    EXPECT_EQ(net.sent_count(), 2u);  // one request, one reply, no retries
}

TEST(RequestTest, RetriesReuseTheSameIdAndDeliverAtMostOnce) {
    network net{42};
    auto& a = net.add_node("A");
    auto& b = net.add_node("B");
    net.link(a.id(), b.id(), {.latency = 2s});

    int served = 0;
    b.serve<hello, world>([&served](const hello&) {
        ++served;
        return world{"ok"};
    });

    int responses = 0;
    std::chrono::nanoseconds response_at{0};
    a.request<hello, world>(b.id(), hello{"reim"})
        .retry_every(1s)
        .on_response([&](const world&) {
            ++responses;
            response_at = net.now();
        });
    net.run_until(10s);

    // Retries at 1s, 2s, 3s all went out before the first response
    // arrived at 4s (2s each way); each retry was served, each reply
    // arrived, but the callback fired exactly once, at 4s.
    EXPECT_EQ(served, 4);
    EXPECT_EQ(responses, 1);
    EXPECT_EQ(response_at, 4s);
    EXPECT_EQ(net.sent_count(), 8u);
    EXPECT_EQ(net.delivered_count(), 8u);
}

TEST(RequestTest, RetryEveryTwiceThrows) {
    network net{42};
    auto& a = net.add_node("A");
    auto& b = net.add_node("B");
    net.link(a.id(), b.id(), {});
    auto handle = a.request<hello, world>(b.id(), hello{"x"});
    handle.retry_every(1s);
    EXPECT_THROW(handle.retry_every(1s), std::logic_error);
}

TEST(RequestTest, TimeoutAfterTwiceThrows) {
    network net{42};
    auto& a = net.add_node("A");
    auto& b = net.add_node("B");
    net.link(a.id(), b.id(), {});
    auto handle = a.request<hello, world>(b.id(), hello{"x"});
    handle.timeout_after(1s);
    EXPECT_THROW(handle.timeout_after(1s), std::logic_error);
}

TEST(RequestTest, NonPositiveRetryAndTimeoutThrow) {
    network net{42};
    auto& a = net.add_node("A");
    auto& b = net.add_node("B");
    net.link(a.id(), b.id(), {});
    auto handle = a.request<hello, world>(b.id(), hello{"x"});
    EXPECT_THROW(handle.retry_every(0s), std::invalid_argument);
    EXPECT_THROW(handle.timeout_after(-1s), std::invalid_argument);
}

TEST(RequestTest, OnResponseReplacesThePreviousCallback) {
    network net{42};
    auto& a = net.add_node("A");
    auto& b = net.add_node("B");
    net.link(a.id(), b.id(), {});
    b.serve<hello, world>([](const hello&) { return world{"ok"}; });

    int first = 0;
    int second = 0;
    a.request<hello, world>(b.id(), hello{"x"})
        .on_response([&first](const world&) { ++first; })
        .on_response([&second](const world&) { ++second; });
    net.run_until(1s);

    EXPECT_EQ(first, 0);
    EXPECT_EQ(second, 1);
}

TEST(RequestTest, RequestToCrashedNodeTimesOut) {
    network net{42};
    auto& a = net.add_node("A");
    auto& b = net.add_node("B");
    net.link(a.id(), b.id(), {});
    net.crash(b.id());

    int timeouts = 0;
    a.request<hello, world>(b.id(), hello{"x"})
        .timeout_after(2s)
        .on_timeout([&timeouts] { ++timeouts; });
    net.run_until(5s);

    EXPECT_EQ(timeouts, 1);
    EXPECT_EQ(net.dropped_count(), 1u);
}

TEST(RequestTest, RequestFromCrashedNodeThrows) {
    network net{42};
    auto& a = net.add_node("A");
    auto& b = net.add_node("B");
    net.link(a.id(), b.id(), {});
    net.crash(a.id());
    EXPECT_THROW((a.request<hello, world>(b.id(), hello{"x"})), std::logic_error);
}

TEST(RequestTest, RequestWithoutLinkThrows) {
    network net{42};
    auto& a = net.add_node("A");
    auto& b = net.add_node("B");
    EXPECT_THROW((a.request<hello, world>(b.id(), hello{"x"})), std::invalid_argument);
}

TEST(RequestTest, ScenarioServeAndRequest) {
    scenario sc{"ping/pong"};
    sc.seed(42).nodes("A", "B").link("A", "B", {.latency = 50ms});
    sc.serve<hello, world>("B",
                           [](const hello& h) { return world{"hello, " + h.name}; });

    std::vector<world> responses;
    sc.request<hello, world>("A", "B", hello{"reim"})
        .retry_every(1s)
        .timeout_after(10s)
        .on_response([&responses](const world& w) { responses.push_back(w); });
    sc.run_until(1s);

    EXPECT_EQ(responses, (std::vector<world>{{"hello, reim"}}));
    sc.expect_sent(2).expect_delivered(2).expect_dropped(0);
}

TEST(RequestTest, ResponseTagMismatchThrowsDecodeError) {
    network net{42};
    auto& a = net.add_node("A");
    auto& b = net.add_node("B");
    net.link(a.id(), b.id(), {});

    // B serves hello -> world, but A asks for hello -> other.
    b.serve<hello, world>([](const hello&) { return world{"wrong type"}; });
    a.request<hello, other>(b.id(), hello{"x"});
    EXPECT_THROW(net.run_until(1s), decode_error);
}

TEST(RequestTest, RequestIdsCollidingAcrossNodesStillRoute) {
    network net{42};
    auto& a = net.add_node("A");
    auto& b = net.add_node("B");
    net.link(a.id(), b.id(), {});

    // Both nodes serve hello -> world AND both make a first request, so
    // both requests carry id 0 (request ids are per-node counters). The
    // envelope kind tag is what keeps A's request from being mistaken for
    // a response to B's pending id 0 (and vice versa).
    a.serve<hello, world>([](const hello& h) { return world{"from A: " + h.name}; });
    b.serve<hello, world>([](const hello& h) { return world{"from B: " + h.name}; });

    std::vector<world> a_responses;
    std::vector<world> b_responses;
    a.request<hello, world>(b.id(), hello{"to B"})
        .on_response([&a_responses](const world& w) { a_responses.push_back(w); });
    b.request<hello, world>(a.id(), hello{"to A"})
        .on_response([&b_responses](const world& w) { b_responses.push_back(w); });
    net.run_until(1s);

    EXPECT_EQ(a_responses, (std::vector<world>{{"from B: to B"}}));
    EXPECT_EQ(b_responses, (std::vector<world>{{"from A: to A"}}));
}

TEST(RequestTest, SelfRequestWorks) {
    network net{42};
    auto& a = net.add_node("A");
    a.serve<hello, world>([](const hello& h) { return world{"self: " + h.name}; });

    std::vector<world> responses;
    a.request<hello, world>(a.id(), hello{"me"})
        .on_response([&responses](const world& w) { responses.push_back(w); });
    net.run_until(1s);

    EXPECT_EQ(responses, (std::vector<world>{{"self: me"}}));
}

TEST(RequestTest, PlainSendDoesNotTriggerPendingRequest) {
    network net{42};
    auto& a = net.add_node("A");
    auto& b = net.add_node("B");
    net.link(a.id(), b.id(), {});

    // A has a pending request expecting a world response, and a plain
    // on<world> handler. B sends a PLAIN world message: it must reach the
    // typed handler, never the pending request (the envelope kind tag is
    // what separates them).
    int plain_worlds = 0;
    a.on<world>([&plain_worlds](const world&) { ++plain_worlds; });
    int responses = 0;
    int timeouts = 0;
    a.request<hello, world>(b.id(), hello{"x"})
        .timeout_after(2s)
        .on_response([&responses](const world&) { ++responses; })
        .on_timeout([&timeouts] { ++timeouts; });
    b.send<world>(a.id(), world{"plain, not a response"});
    net.run_until(5s);

    EXPECT_EQ(plain_worlds, 1);
    EXPECT_EQ(responses, 0);
    EXPECT_EQ(timeouts, 1);
}

}  // namespace
