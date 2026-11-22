#include <chrono>
#include <cstddef>
#include <stdexcept>
#include <vector>

#include <gtest/gtest.h>

#include "simulnetx/network.hxx"

namespace {

using namespace std::chrono_literals;

TEST(CrashTest, SendToCrashedNodeDroppedAtSendTime) {
    simulnetx::network net(1);
    auto& a = net.add_node("A");
    auto& b = net.add_node("B");
    int got = 0;
    b.on_message([&](const simulnetx::message&) { ++got; });
    net.link(a.id(), b.id(), {});
    net.crash(b.id());
    EXPECT_TRUE(net.is_crashed(b.id()));
    a.send(b.id(), "x");
    net.run_for(1s);
    EXPECT_EQ(got, 0);
    net.sent_count().equal(1);
    net.delivered_count().equal(0);
    net.dropped_count().equal(1);
}

TEST(CrashTest, InFlightMessageDroppedWhenDestinationCrashes) {
    simulnetx::network net(1);
    auto& a = net.add_node("A");
    auto& b = net.add_node("B");
    int got = 0;
    b.on_message([&](const simulnetx::message&) { ++got; });
    net.link(a.id(), b.id(), {.latency = 100ms});
    a.send(b.id(), "x");
    net.crash(b.id());  // before the message arrives
    net.run_for(1s);
    EXPECT_EQ(got, 0);
    net.dropped_count().equal(1);
}

TEST(CrashTest, MessageDeliveredIfRestartBeforeArrival) {
    simulnetx::network net(1);
    auto& a = net.add_node("A");
    auto& b = net.add_node("B");
    int got = 0;
    b.on_message([&](const simulnetx::message&) { ++got; });
    net.link(a.id(), b.id(), {.latency = 100ms});
    a.send(b.id(), "x");
    net.crash(b.id());
    // Restart well before the 100ms arrival: the message survives the crash.
    net.after(50ms, [&] { net.restart(b.id()); });
    net.run_for(1s);
    EXPECT_EQ(got, 1);
    EXPECT_FALSE(net.is_crashed(b.id()));
    net.delivered_count().equal(1);
    net.dropped_count().equal(0);
}

TEST(CrashTest, CrashedNodeSendThrowsLogicError) {
    simulnetx::network net(1);
    auto& a = net.add_node("A");
    auto& b = net.add_node("B");
    net.link(a.id(), b.id(), {});
    net.crash(a.id());
    EXPECT_THROW(a.send(b.id(), "x"), std::logic_error);
    EXPECT_THROW(a.send(b.id(), std::vector<std::byte>{std::byte{0x01}}), std::logic_error);
    // A failed send is not counted.
    net.sent_count().equal(0);
    // After restart, sending works again.
    net.restart(a.id());
    EXPECT_NO_THROW(a.send(b.id(), "x"));
    net.run_for(1s);
    net.delivered_count().equal(1);
}

TEST(CrashTest, SelfSendFromCrashedNodeThrows) {
    simulnetx::network net(1);
    auto& a = net.add_node("A");
    net.crash(a.id());
    EXPECT_THROW(a.send(a.id(), "me"), std::logic_error);
    net.sent_count().equal(0);
}

TEST(CrashTest, SelfSendDroppedIfNodeCrashesBeforeDelivery) {
    simulnetx::network net(1);
    auto& a = net.add_node("A");
    int got = 0;
    a.on_message([&](const simulnetx::message&) { ++got; });
    a.send(a.id(), "me");  // scheduled at the current virtual time
    net.crash(a.id());     // ... but the node dies before the run processes it
    net.run_for(1s);
    EXPECT_EQ(got, 0);
    net.sent_count().equal(1);
    net.dropped_count().equal(1);
}

TEST(CrashTest, CrashedNodeAfterThrowsLogicError) {
    simulnetx::network net(1);
    auto& a = net.add_node("A");
    net.crash(a.id());
    EXPECT_THROW(a.after(10ms, [] {}), std::logic_error);
    net.restart(a.id());
    EXPECT_NO_THROW(a.after(10ms, [] {}));
}

TEST(CrashTest, TimerCancelledAtCrash) {
    simulnetx::network net(1);
    auto& a = net.add_node("A");
    int fired = 0;
    a.after(100ms, [&] { ++fired; });
    net.crash(a.id());
    net.run_for(1s);
    EXPECT_EQ(fired, 0);
}

TEST(CrashTest, TimerNotRestoredAfterRestart) {
    simulnetx::network net(1);
    auto& a = net.add_node("A");
    int fired = 0;
    a.after(100ms, [&] { ++fired; });
    net.crash(a.id());
    net.restart(a.id());  // restart BEFORE the 100ms fire time
    net.run_for(1s);
    EXPECT_EQ(fired, 0);
    // A timer scheduled after the restart does fire.
    a.after(100ms, [&] { ++fired; });
    net.run_for(1s);
    EXPECT_EQ(fired, 1);
}

TEST(CrashTest, TimerScheduledByLiveNodeSurvivesOtherNodeCrash) {
    simulnetx::network net(1);
    auto& a = net.add_node("A");
    auto& b = net.add_node("B");
    int fired = 0;
    a.after(100ms, [&] { ++fired; });
    net.crash(b.id());  // an unrelated node dies
    net.run_for(1s);
    EXPECT_EQ(fired, 1);
}

TEST(CrashTest, HandlerPreservedAcrossRestart) {
    simulnetx::network net(1);
    auto& a = net.add_node("A");
    auto& b = net.add_node("B");
    int got = 0;
    b.on_message([&](const simulnetx::message&) { ++got; });
    net.link(a.id(), b.id(), {});
    net.crash(b.id());
    net.restart(b.id());
    a.send(b.id(), "x");
    net.run_for(1s);
    EXPECT_EQ(got, 1);
    net.delivered_count().equal(1);
}

TEST(CrashTest, CrashRestartAreIdempotentNoOps) {
    simulnetx::network net(1);
    auto& a = net.add_node("A");
    EXPECT_FALSE(net.is_crashed(a.id()));
    EXPECT_NO_THROW(net.restart(a.id()));  // live node: no-op
    net.crash(a.id());
    EXPECT_TRUE(net.is_crashed(a.id()));
    EXPECT_NO_THROW(net.crash(a.id()));  // already crashed: no-op
    EXPECT_TRUE(net.is_crashed(a.id()));
    net.restart(a.id());
    EXPECT_FALSE(net.is_crashed(a.id()));
    EXPECT_NO_THROW(net.restart(a.id()));
    EXPECT_FALSE(net.is_crashed(a.id()));
}

TEST(CrashTest, UnknownIdThrows) {
    simulnetx::network net(1);
    net.add_node("A");
    EXPECT_THROW(net.crash(99), std::invalid_argument);
    EXPECT_THROW(net.restart(99), std::invalid_argument);
    EXPECT_THROW(net.is_crashed(99), std::invalid_argument);
}

TEST(CrashTest, RestartDoesNotHealPartition) {
    simulnetx::network net(1);
    auto& a = net.add_node("A");
    auto& b = net.add_node("B");
    int got = 0;
    b.on_message([&](const simulnetx::message&) { ++got; });
    net.link(a.id(), b.id(), {});
    net.partition({a.id()}, {b.id()});
    net.crash(b.id());
    net.restart(b.id());
    a.send(b.id(), "x");
    net.run_for(1s);
    EXPECT_EQ(got, 0);  // still partitioned
    net.dropped_count().equal(1);
    net.heal();
    a.send(b.id(), "x");
    net.run_for(1s);
    EXPECT_EQ(got, 1);
}

TEST(CrashTest, NetworkTimerSurvivesNodeCrash) {
    simulnetx::network net(1);
    auto& a = net.add_node("A");
    int fired = 0;
    net.after(100ms, [&] { ++fired; });
    net.crash(a.id());
    net.run_for(1s);
    EXPECT_EQ(fired, 1);
    // Network timers can restart the node they outlived.
    net.schedule_at(1500ms, [&] { net.restart(a.id()); });
    net.run_for(1s);
    EXPECT_FALSE(net.is_crashed(a.id()));
}

// Same crash scenario twice: identical stats and identical arrival log.
struct crash_outcome {
    std::size_t sent;
    std::size_t delivered;
    std::size_t dropped;
    std::vector<long long> arrivals;
    bool operator==(const crash_outcome&) const = default;
};

crash_outcome run_crash_scenario(std::uint64_t seed) {
    simulnetx::network net(seed);
    auto& a = net.add_node("A");
    auto& b = net.add_node("B");
    crash_outcome result;
    b.on_message([&](const simulnetx::message&) { result.arrivals.push_back(net.now().count()); });
    net.link(a.id(), b.id(), {.latency = 10ms, .jitter = 5ms, .loss_rate = 0.2});
    net.after(300ms, [&] { net.crash(b.id()); });
    net.after(700ms, [&] { net.restart(b.id()); });
    for (int i = 0; i < 50; ++i) {
        net.after(std::chrono::milliseconds(i * 20), [&] { a.send(b.id(), "x"); });
    }
    net.run_for(2s);
    result.sent = net.sent_count();
    result.delivered = net.delivered_count();
    result.dropped = net.dropped_count();
    return result;
}

TEST(CrashTest, CrashScenarioIsDeterministic) {
    const crash_outcome first = run_crash_scenario(42);
    const crash_outcome second = run_crash_scenario(42);
    EXPECT_EQ(first, second);
    EXPECT_GT(first.sent, 0u);
    EXPECT_GT(first.dropped, 0u);  // the crash window must have dropped some
    EXPECT_GT(first.delivered, 0u);
}

TEST(CrashTest, CrashOfUninvolvedNodeDoesNotPerturbRng) {
    // Crashing a node that never sends or receives must not change the RNG
    // stream: A <-> B traffic is identical with and without the crash.
    const auto run = [](bool crash_c) {
        simulnetx::network net(42);
        auto& a = net.add_node("A");
        auto& b = net.add_node("B");
        auto& c = net.add_node("C");
        std::vector<long long> arrivals;
        b.on_message([&](const simulnetx::message&) { arrivals.push_back(net.now().count()); });
        net.link(a.id(), b.id(), {.latency = 10ms, .jitter = 5ms, .loss_rate = 0.2,
                                  .dup_rate = 0.1, .reorder_rate = 0.3});
        if (crash_c) {
            net.after(100ms, [&] { net.crash(c.id()); });
            net.after(500ms, [&] { net.restart(c.id()); });
        }
        for (int i = 0; i < 50; ++i) {
            a.send(b.id(), "x");
        }
        net.run_for(2s);
        return std::pair{arrivals, std::size_t(net.delivered_count())};
    };
    EXPECT_EQ(run(false), run(true));
}

}  // namespace
