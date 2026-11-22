#include <chrono>
#include <cstddef>
#include <stdexcept>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "simulnetx/network.hxx"

namespace {

using namespace std::chrono_literals;

TEST(NetworkTest, ZeroLossDeliveryHasExactCounts) {
    simulnetx::network net(1);
    auto& a = net.add_node("A");
    auto& b = net.add_node("B");
    net.link(a.id(), b.id(), {});
    for (int i = 0; i < 10; ++i) {
        a.send(b.id(), "x");
    }
    net.run_for(1s);
    net.sent_count().equal(10);
    net.delivered_count().equal(10);
    net.dropped_count().equal(0);
}

TEST(NetworkTest, LatencyDelaysDelivery) {
    simulnetx::network net(1);
    auto& a = net.add_node("A");
    auto& b = net.add_node("B");
    int received = 0;
    b.on_message([&](const simulnetx::message&) { ++received; });
    net.link(a.id(), b.id(), {.latency = 50ms});
    a.send(b.id(), "x");
    net.run_for(49ms);
    EXPECT_EQ(received, 0);
    net.run_for(1ms);
    EXPECT_EQ(received, 1);
}

TEST(NetworkTest, NodeReferencesStayValidAcrossAddNode) {
    simulnetx::network net(1);
    auto& a = net.add_node("A");
    const simulnetx::node* ptr = &a;
    for (int i = 0; i < 100; ++i) {
        net.add_node("extra");
    }
    EXPECT_EQ(&a, ptr);
    EXPECT_EQ(a.id(), 0u);
    EXPECT_EQ(a.name(), "A");
}

TEST(NetworkTest, SelfSendDeliversAtCurrentVirtualTime) {
    simulnetx::network net(1);
    auto& a = net.add_node("A");
    long long arrived_at = -1;
    a.on_message([&](const simulnetx::message&) { arrived_at = net.now().count(); });
    net.run_for(123ms);
    a.send(a.id(), "me");
    net.run_for(0ns);
    EXPECT_EQ(arrived_at, 123000000LL);
    net.sent_count().equal(1);
    net.delivered_count().equal(1);
}

TEST(NetworkTest, NoLinkSendThrows) {
    simulnetx::network net(1);
    auto& a = net.add_node("A");
    auto& b = net.add_node("B");
    EXPECT_THROW(a.send(b.id(), "x"), std::invalid_argument);
    // A failed send is not counted.
    net.sent_count().equal(0);
}

TEST(NetworkTest, SendToUnknownNodeThrows) {
    simulnetx::network net(1);
    auto& a = net.add_node("A");
    EXPECT_THROW(a.send(99, "x"), std::invalid_argument);
}

TEST(NetworkTest, InvalidLinkConfigThrows) {
    simulnetx::network net(1);
    auto& a = net.add_node("A");
    auto& b = net.add_node("B");
    EXPECT_THROW(net.link(a.id(), b.id(), {.loss_rate = 1.5}), std::invalid_argument);
    EXPECT_THROW(net.link(a.id(), b.id(), {.loss_rate = -0.1}), std::invalid_argument);
    EXPECT_THROW(net.link(a.id(), b.id(), {.dup_rate = 2.0}), std::invalid_argument);
    EXPECT_THROW(net.link(a.id(), b.id(), {.reorder_rate = -1.0}), std::invalid_argument);
    EXPECT_THROW(net.link(a.id(), b.id(), {.latency = -1ms}), std::invalid_argument);
    EXPECT_THROW(net.link(a.id(), b.id(), {.jitter = -1ms}), std::invalid_argument);
    // Boundary rates are valid.
    EXPECT_NO_THROW(net.link(a.id(), b.id(), {.loss_rate = 1.0}));
    EXPECT_NO_THROW(net.link(a.id(), b.id(), {.loss_rate = 0.0}));
}

TEST(NetworkTest, SelfLinkThrows) {
    simulnetx::network net(1);
    auto& a = net.add_node("A");
    EXPECT_THROW(net.link(a.id(), a.id(), {}), std::invalid_argument);
}

TEST(NetworkTest, LinkUnknownNodeThrows) {
    simulnetx::network net(1);
    auto& a = net.add_node("A");
    EXPECT_THROW(net.link(a.id(), 42, {}), std::invalid_argument);
}

TEST(NetworkTest, RunUntilPastThrows) {
    simulnetx::network net(1);
    net.run_for(1s);
    EXPECT_THROW(net.run_until(500ms), std::invalid_argument);
}

TEST(NetworkTest, RunForNegativeThrows) {
    simulnetx::network net(1);
    EXPECT_THROW(net.run_for(-1ms), std::invalid_argument);
}

TEST(NetworkTest, TimerNegativeDelayThrows) {
    simulnetx::network net(1);
    auto& a = net.add_node("A");
    EXPECT_THROW(a.after(-1ms, [] {}), std::invalid_argument);
}

TEST(NetworkTest, TimerFiresAtExactVirtualTime) {
    simulnetx::network net(1);
    auto& a = net.add_node("A");
    std::vector<long long> fired_at;
    a.after(100ms, [&] {
        fired_at.push_back(net.now().count());
        // A timer may schedule another timer.
        a.after(50ms, [&] { fired_at.push_back(net.now().count()); });
    });
    net.run_for(200ms);
    ASSERT_EQ(fired_at.size(), 2u);
    EXPECT_EQ(fired_at[0], 100000000LL);
    EXPECT_EQ(fired_at[1], 150000000LL);
}

TEST(NetworkTest, NetworkTimerFiresAtExactVirtualTime) {
    simulnetx::network net(1);
    std::vector<long long> fired_at;
    net.after(100ms, [&] {
        fired_at.push_back(net.now().count());
        net.after(50ms, [&] { fired_at.push_back(net.now().count()); });
    });
    net.run_for(200ms);
    ASSERT_EQ(fired_at.size(), 2u);
    EXPECT_EQ(fired_at[0], 100000000LL);
    EXPECT_EQ(fired_at[1], 150000000LL);
}

TEST(NetworkTest, NetworkTimerNeedsNoNode) {
    // Unlike node timers, network timers work on an empty network.
    simulnetx::network net(1);
    int fired = 0;
    net.after(10ms, [&] { ++fired; });
    net.schedule_at(20ms, [&] { ++fired; });
    net.run_for(100ms);
    EXPECT_EQ(fired, 2);
}

TEST(NetworkTest, NetworkScheduleAtInPastThrows) {
    simulnetx::network net(1);
    net.run_for(1s);
    EXPECT_THROW(net.schedule_at(500ms, [] {}), std::invalid_argument);
    // Scheduling exactly at now is fine.
    EXPECT_NO_THROW(net.schedule_at(1s, [] {}));
}

TEST(NetworkTest, NetworkAfterNegativeDelayThrows) {
    simulnetx::network net(1);
    EXPECT_THROW(net.after(-1ms, [] {}), std::invalid_argument);
}

TEST(NetworkTest, ZeroDelayTimerFiresAtCurrentTime) {
    simulnetx::network net(1);
    auto& a = net.add_node("A");
    long long fired_at = -1;
    net.run_for(77ms);
    a.after(0ns, [&] { fired_at = net.now().count(); });
    net.run_for(0ns);
    EXPECT_EQ(fired_at, 77000000LL);
}

TEST(NetworkTest, RelinkReplacesConfig) {
    simulnetx::network net(1);
    auto& a = net.add_node("A");
    auto& b = net.add_node("B");
    net.link(a.id(), b.id(), {.loss_rate = 1.0});
    net.link(a.id(), b.id(), {.loss_rate = 0.0});
    a.send(b.id(), "x");
    net.run_for(1s);
    net.delivered_count().equal(1);
    net.dropped_count().equal(0);
}

TEST(NetworkTest, HandlerReplacement) {
    simulnetx::network net(1);
    auto& a = net.add_node("A");
    auto& b = net.add_node("B");
    int first = 0;
    int second = 0;
    b.on_message([&](const simulnetx::message&) { ++first; });
    b.on_message([&](const simulnetx::message&) { ++second; });
    net.link(a.id(), b.id(), {});
    a.send(b.id(), "x");
    net.run_for(1s);
    EXPECT_EQ(first, 0);
    EXPECT_EQ(second, 1);
}

TEST(NetworkTest, NoHandlerStillCountsAsDelivered) {
    simulnetx::network net(1);
    auto& a = net.add_node("A");
    auto& b = net.add_node("B");
    net.link(a.id(), b.id(), {});
    a.send(b.id(), "x");
    EXPECT_NO_THROW(net.run_for(1s));
    net.delivered_count().equal(1);
}

TEST(NetworkTest, PayloadRoundTripsExactly) {
    simulnetx::network net(1);
    auto& a = net.add_node("A");
    auto& b = net.add_node("B");
    std::vector<std::byte> seen;
    simulnetx::node_id seen_from = 0;
    simulnetx::node_id seen_to = 0;
    b.on_message([&](const simulnetx::message& m) {
        seen = m.payload;
        seen_from = m.from;
        seen_to = m.to;
    });
    net.link(a.id(), b.id(), {});
    const std::vector<std::byte> payload{std::byte{0x00}, std::byte{0xFF}, std::byte{0x42}};
    a.send(b.id(), payload);
    net.run_for(1s);
    EXPECT_EQ(seen, payload);
    EXPECT_EQ(seen_from, a.id());
    EXPECT_EQ(seen_to, b.id());
}

TEST(NetworkTest, StringViewOverloadRoundTrips) {
    simulnetx::network net(1);
    auto& a = net.add_node("A");
    auto& b = net.add_node("B");
    std::string seen;
    b.on_message([&](const simulnetx::message& m) {
        seen.assign(reinterpret_cast<const char*>(m.payload.data()), m.payload.size());
    });
    net.link(a.id(), b.id(), {});
    a.send(b.id(), "hello");
    net.run_for(1s);
    EXPECT_EQ(seen, "hello");
}

TEST(NetworkTest, TotalLossDropsEverything) {
    simulnetx::network net(1);
    auto& a = net.add_node("A");
    auto& b = net.add_node("B");
    net.link(a.id(), b.id(), {.loss_rate = 1.0});
    for (int i = 0; i < 50; ++i) {
        a.send(b.id(), "x");
    }
    net.run_for(1s);
    net.sent_count().equal(50);
    net.delivered_count().equal(0);
    net.dropped_count().equal(50);
}

TEST(NetworkTest, FullDupDoublesDeliveries) {
    simulnetx::network net(1);
    auto& a = net.add_node("A");
    auto& b = net.add_node("B");
    net.link(a.id(), b.id(), {.dup_rate = 1.0});
    for (int i = 0; i < 50; ++i) {
        a.send(b.id(), "x");
    }
    net.run_for(1s);
    net.sent_count().equal(50);
    net.delivered_count().equal(100);
    net.dropped_count().equal(0);
}

}  // namespace
