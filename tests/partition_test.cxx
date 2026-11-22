#include <chrono>
#include <cstddef>
#include <stdexcept>

#include <gtest/gtest.h>

#include "simulnetx/network.hxx"

namespace {

using namespace std::chrono_literals;

TEST(PartitionTest, CrossGroupDroppedSameGroupDelivered) {
    simulnetx::network net(1);
    auto& a = net.add_node("A");
    auto& b = net.add_node("B");
    auto& c = net.add_node("C");
    int got_b = 0;
    int got_c = 0;
    b.on_message([&](const simulnetx::message&) { ++got_b; });
    c.on_message([&](const simulnetx::message&) { ++got_c; });
    net.link(a.id(), b.id(), {});
    net.link(a.id(), c.id(), {});
    net.link(b.id(), c.id(), {});

    net.partition({a.id(), c.id()}, {b.id()});
    a.send(c.id(), "x");  // same group: delivered
    a.send(b.id(), "x");  // cross-group: dropped
    b.send(a.id(), "x");  // cross-group, other direction: dropped
    net.run_for(1s);

    EXPECT_EQ(got_c, 1);
    EXPECT_EQ(got_b, 0);
    net.sent_count().equal(3);
    net.delivered_count().equal(1);
    net.dropped_count().equal(2);
}

TEST(PartitionTest, UnlistedNodesUnaffected) {
    simulnetx::network net(1);
    auto& a = net.add_node("A");
    auto& b = net.add_node("B");
    auto& c = net.add_node("C");
    int got_c = 0;
    c.on_message([&](const simulnetx::message&) { ++got_c; });
    net.link(a.id(), c.id(), {});
    net.link(b.id(), c.id(), {});

    net.partition({a.id()}, {b.id()});
    a.send(c.id(), "x");
    b.send(c.id(), "x");
    net.run_for(1s);
    EXPECT_EQ(got_c, 2);
    net.dropped_count().equal(0);
}

TEST(PartitionTest, HealRestoresDelivery) {
    simulnetx::network net(1);
    auto& a = net.add_node("A");
    auto& b = net.add_node("B");
    int got = 0;
    b.on_message([&](const simulnetx::message&) { ++got; });
    net.link(a.id(), b.id(), {});

    net.partition({a.id()}, {b.id()});
    a.send(b.id(), "x");
    net.run_for(1s);
    EXPECT_EQ(got, 0);

    net.heal();
    a.send(b.id(), "x");
    net.run_for(1s);
    EXPECT_EQ(got, 1);
    // The message dropped while partitioned stays dropped.
    net.dropped_count().equal(1);
}

TEST(PartitionTest, InFlightMessageDroppedWhenPartitionStarts) {
    simulnetx::network net(1);
    auto& a = net.add_node("A");
    auto& b = net.add_node("B");
    int got = 0;
    b.on_message([&](const simulnetx::message&) { ++got; });
    net.link(a.id(), b.id(), {.latency = 100ms});
    a.send(b.id(), "x");
    net.partition({a.id()}, {b.id()});
    net.run_for(1s);
    EXPECT_EQ(got, 0);
    net.dropped_count().equal(1);
}

TEST(PartitionTest, OverlappingGroupsThrow) {
    simulnetx::network net(1);
    auto& a = net.add_node("A");
    auto& b = net.add_node("B");
    EXPECT_THROW(net.partition({a.id()}, {a.id(), b.id()}), std::invalid_argument);
}

TEST(PartitionTest, UnknownNodeInPartitionThrows) {
    simulnetx::network net(1);
    auto& a = net.add_node("A");
    EXPECT_THROW(net.partition({a.id()}, {99}), std::invalid_argument);
    EXPECT_THROW(net.partition({99}, {a.id()}), std::invalid_argument);
}

TEST(PartitionTest, SecondPartitionReplacesFirst) {
    simulnetx::network net(1);
    auto& a = net.add_node("A");
    auto& b = net.add_node("B");
    auto& c = net.add_node("C");
    int got_b = 0;
    int got_c = 0;
    b.on_message([&](const simulnetx::message&) { ++got_b; });
    c.on_message([&](const simulnetx::message&) { ++got_c; });
    net.link(a.id(), b.id(), {});
    net.link(a.id(), c.id(), {});

    net.partition({a.id()}, {b.id()});
    net.partition({a.id()}, {c.id()});  // now A-B is fine, A-C is cut
    a.send(b.id(), "x");
    a.send(c.id(), "x");
    net.run_for(1s);
    EXPECT_EQ(got_b, 1);
    EXPECT_EQ(got_c, 0);
}

}  // namespace
