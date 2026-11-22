#include <chrono>
#include <stdexcept>

#include <gtest/gtest.h>

#include "simulnetx/network.hxx"

namespace {

using namespace std::chrono_literals;

TEST(DirectedTest, DirectedLinkAppliesToOneDirectionOnly) {
    simulnetx::network net(1);
    auto& a = net.add_node("A");
    auto& b = net.add_node("B");
    int got_b = 0;
    b.on_message([&](const simulnetx::message&) { ++got_b; });
    net.link_directed(a.id(), b.id(), {.latency = 10ms});
    a.send(b.id(), "x");
    net.run_for(1s);
    EXPECT_EQ(got_b, 1);
    net.sent_count().equal(1);
    net.delivered_count().equal(1);
    // The reverse direction has no link at all.
    EXPECT_THROW(b.send(a.id(), "x"), std::invalid_argument);
    net.sent_count().equal(1);
}

TEST(DirectedTest, LinkStillSetsBothDirections) {
    simulnetx::network net(1);
    auto& a = net.add_node("A");
    auto& b = net.add_node("B");
    int got_a = 0;
    int got_b = 0;
    a.on_message([&](const simulnetx::message&) { ++got_a; });
    b.on_message([&](const simulnetx::message&) { ++got_b; });
    net.link(a.id(), b.id(), {});
    a.send(b.id(), "x");
    b.send(a.id(), "y");
    net.run_for(1s);
    EXPECT_EQ(got_a, 1);
    EXPECT_EQ(got_b, 1);
}

TEST(DirectedTest, LinkDirectedAfterLinkOverridesOneDirectionOnly) {
    simulnetx::network net(1);
    auto& a = net.add_node("A");
    auto& b = net.add_node("B");
    int got_a = 0;
    int got_b = 0;
    a.on_message([&](const simulnetx::message&) { ++got_a; });
    b.on_message([&](const simulnetx::message&) { ++got_b; });
    net.link(a.id(), b.id(), {.loss_rate = 1.0});
    // Open only A -> B; B -> A stays fully lossy.
    net.link_directed(a.id(), b.id(), {.loss_rate = 0.0});
    a.send(b.id(), "x");
    b.send(a.id(), "y");
    net.run_for(1s);
    EXPECT_EQ(got_b, 1);
    EXPECT_EQ(got_a, 0);
    net.sent_count().equal(2);
    net.delivered_count().equal(1);
    net.dropped_count().equal(1);
}

TEST(DirectedTest, LinkAfterDirectedResetsBothDirections) {
    simulnetx::network net(1);
    auto& a = net.add_node("A");
    auto& b = net.add_node("B");
    int got_a = 0;
    int got_b = 0;
    a.on_message([&](const simulnetx::message&) { ++got_a; });
    b.on_message([&](const simulnetx::message&) { ++got_b; });
    net.link_directed(a.id(), b.id(), {.loss_rate = 1.0});
    net.link(a.id(), b.id(), {.loss_rate = 0.0});
    a.send(b.id(), "x");
    b.send(a.id(), "y");
    net.run_for(1s);
    EXPECT_EQ(got_a, 1);
    EXPECT_EQ(got_b, 1);
}

TEST(DirectedTest, LinkDirectedReplacesOnlyThatDirection) {
    simulnetx::network net(1);
    auto& a = net.add_node("A");
    auto& b = net.add_node("B");
    long long arrived_a = -1;
    long long arrived_b = -1;
    a.on_message([&](const simulnetx::message&) { arrived_a = net.now().count(); });
    b.on_message([&](const simulnetx::message&) { arrived_b = net.now().count(); });
    net.link_directed(a.id(), b.id(), {.latency = 10ms});
    net.link_directed(b.id(), a.id(), {.latency = 20ms});
    // Replacing A -> B must not touch B -> A's 20ms latency.
    net.link_directed(a.id(), b.id(), {.latency = 100ms});
    a.send(b.id(), "x");
    b.send(a.id(), "y");
    net.run_for(1s);
    EXPECT_EQ(arrived_b, 100000000LL);
    EXPECT_EQ(arrived_a, 20000000LL);
}

TEST(DirectedTest, AsymmetricConfigsAreIndependent) {
    simulnetx::network net(1);
    auto& a = net.add_node("A");
    auto& b = net.add_node("B");
    int got_a = 0;
    int got_b = 0;
    a.on_message([&](const simulnetx::message&) { ++got_a; });
    b.on_message([&](const simulnetx::message&) { ++got_b; });
    net.link_directed(a.id(), b.id(), {.loss_rate = 1.0});
    net.link_directed(b.id(), a.id(), {.loss_rate = 0.0});
    a.send(b.id(), "x");
    b.send(a.id(), "y");
    net.run_for(1s);
    EXPECT_EQ(got_b, 0);
    EXPECT_EQ(got_a, 1);
}

TEST(DirectedTest, LinkDirectedValidationThrows) {
    simulnetx::network net(1);
    auto& a = net.add_node("A");
    auto& b = net.add_node("B");
    EXPECT_THROW(net.link_directed(a.id(), 42, {}), std::invalid_argument);
    EXPECT_THROW(net.link_directed(42, b.id(), {}), std::invalid_argument);
    EXPECT_THROW(net.link_directed(a.id(), a.id(), {}), std::invalid_argument);
    EXPECT_THROW(net.link_directed(a.id(), b.id(), {.loss_rate = 1.5}), std::invalid_argument);
    EXPECT_THROW(net.link_directed(a.id(), b.id(), {.latency = -1ms}), std::invalid_argument);
}

TEST(DirectedTest, PartitionIsDirectionIndependent) {
    simulnetx::network net(1);
    auto& a = net.add_node("A");
    auto& b = net.add_node("B");
    int got_a = 0;
    int got_b = 0;
    a.on_message([&](const simulnetx::message&) { ++got_a; });
    b.on_message([&](const simulnetx::message&) { ++got_b; });
    net.link_directed(a.id(), b.id(), {});
    net.link_directed(b.id(), a.id(), {});
    net.partition({a.id()}, {b.id()});
    a.send(b.id(), "x");
    b.send(a.id(), "y");
    net.run_for(1s);
    EXPECT_EQ(got_a, 0);
    EXPECT_EQ(got_b, 0);
    net.dropped_count().equal(2);
}

}  // namespace
