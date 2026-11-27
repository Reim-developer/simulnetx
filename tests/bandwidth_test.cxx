// Link bandwidth + FIFO transmission queue (see link_config.hxx and
// docs/bandwidth.md): serialization delay is floor(N * 8 * 1e9 /
// bandwidth_bps) ns, messages queue strictly FIFO per directed link,
// delivery is finish + latency (+ jitter / reorder delay).

#include <chrono>
#include <cstddef>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "simulnetx/network.hxx"
#include "simulnetx/scenario.hxx"

namespace {

using namespace std::chrono_literals;
using simulnetx::event_kind;

// Delivery times of a run, in order, from the trace.
std::vector<std::chrono::nanoseconds> delivered_times(const simulnetx::network& net) {
    std::vector<std::chrono::nanoseconds> times;
    for (const auto& e : net.trace()) {
        if (e.kind == event_kind::delivered) {
            times.push_back(e.at);
        }
    }
    return times;
}

std::vector<std::byte> bytes_of(std::size_t n) {
    return std::vector<std::byte>(n, std::byte{'x'});
}

TEST(BandwidthTest, PinnedDelays) {
    // 10 bytes @ 80 bps = 1s; 125 bytes @ 1000 bps = 1s; 1000 bytes @
    // 1000 bps = 8s. All exact divisions.
    {
        simulnetx::network net(0);
        auto& a = net.add_node("A");
        auto& b = net.add_node("B");
        net.link(a.id(), b.id(), {.bandwidth_bps = 80});
        a.send(b.id(), bytes_of(10));
        net.run_for(2s);
        ASSERT_EQ(delivered_times(net).size(), 1U);
        EXPECT_EQ(delivered_times(net)[0], 1s);
    }
    {
        simulnetx::network net(0);
        auto& a = net.add_node("A");
        auto& b = net.add_node("B");
        net.link(a.id(), b.id(), {.bandwidth_bps = 1000});
        a.send(b.id(), bytes_of(125));
        net.run_for(2s);
        ASSERT_EQ(delivered_times(net).size(), 1U);
        EXPECT_EQ(delivered_times(net)[0], 1s);
    }
    {
        simulnetx::network net(0);
        auto& a = net.add_node("A");
        auto& b = net.add_node("B");
        net.link(a.id(), b.id(), {.bandwidth_bps = 1000});
        a.send(b.id(), bytes_of(1000));
        net.run_for(10s);
        ASSERT_EQ(delivered_times(net).size(), 1U);
        EXPECT_EQ(delivered_times(net)[0], 8s);
    }
}

TEST(BandwidthTest, NonExactDelayFloors) {
    // 1 byte @ 3 bps = floor(8e9 / 3) = 2666666666 ns, not 2666666667.
    simulnetx::network net(0);
    auto& a = net.add_node("A");
    auto& b = net.add_node("B");
    net.link(a.id(), b.id(), {.bandwidth_bps = 3});
    a.send(b.id(), "x");
    net.run_for(3s);
    ASSERT_EQ(delivered_times(net).size(), 1U);
    EXPECT_EQ(delivered_times(net)[0], std::chrono::nanoseconds{2666666666LL});
}

TEST(BandwidthTest, ThreeMessagesQueueFifo) {
    // Three 1000-byte messages sent at t=0 over a 1000 Bps link with 0
    // latency: arrivals at t=8s, t=16s, t=24s. That spacing IS the test.
    simulnetx::network net(0);
    auto& a = net.add_node("A");
    auto& b = net.add_node("B");
    net.link(a.id(), b.id(), {.bandwidth_bps = 1000});
    a.send(b.id(), bytes_of(1000));
    a.send(b.id(), bytes_of(1000));
    a.send(b.id(), bytes_of(1000));
    net.run_for(30s);
    EXPECT_EQ(delivered_times(net),
              (std::vector<std::chrono::nanoseconds>{8s, 16s, 24s}));
}

TEST(BandwidthTest, LargePayloadNoOverflow) {
    // 1 MB @ 1 Bps = 8,000,000 s of virtual time - virtual, so it costs
    // nothing. The naive product (bytes * 8 * 1e9) overflows u64 past
    // ~2.3 GB; the division-first split must not.
    simulnetx::network net(0);
    auto& a = net.add_node("A");
    auto& b = net.add_node("B");
    net.link(a.id(), b.id(), {.bandwidth_bps = 1});
    a.send(b.id(), bytes_of(1'000'000));
    net.run_for(std::chrono::nanoseconds{8'000'000'000'000'000LL});
    ASSERT_EQ(delivered_times(net).size(), 1U);
    EXPECT_EQ(delivered_times(net)[0],
              std::chrono::nanoseconds{8'000'000'000'000'000LL});
}

TEST(BandwidthTest, UnlimitedIsDefault) {
    // bandwidth_bps 0 (the default) means no delay and no queueing:
    // arrival is latency only, however large the payload.
    simulnetx::network net(0);
    auto& a = net.add_node("A");
    auto& b = net.add_node("B");
    net.link(a.id(), b.id(), {.latency = 10ms});
    a.send(b.id(), bytes_of(100000));
    net.run_for(1s);
    ASSERT_EQ(delivered_times(net).size(), 1U);
    EXPECT_EQ(delivered_times(net)[0], 10ms);
}

TEST(BandwidthTest, LatencyAddsAfterTransmission) {
    // Delivery is finish + latency: 1000 bytes @ 1000 Bps (8s) plus
    // 2s latency arrives at 10s.
    simulnetx::network net(0);
    auto& a = net.add_node("A");
    auto& b = net.add_node("B");
    net.link(a.id(), b.id(), {.latency = 2s, .bandwidth_bps = 1000});
    a.send(b.id(), bytes_of(1000));
    net.run_for(12s);
    ASSERT_EQ(delivered_times(net).size(), 1U);
    EXPECT_EQ(delivered_times(net)[0], 10s);
}

TEST(BandwidthTest, DuplicateIsASecondTransmission) {
    // A duplicated copy consumes bandwidth and queues behind the
    // original: 1000 bytes @ 1000 Bps, dup_rate 1.0 - the original
    // arrives at 8s, the copy at 16s + 1ns.
    simulnetx::network net(0);
    auto& a = net.add_node("A");
    auto& b = net.add_node("B");
    net.link(a.id(), b.id(), {.dup_rate = 1.0, .bandwidth_bps = 1000});
    a.send(b.id(), bytes_of(1000));
    net.run_for(20s);
    EXPECT_EQ(delivered_times(net),
              (std::vector<std::chrono::nanoseconds>{
                  8s, std::chrono::nanoseconds{16'000'000'001LL}}));
}

TEST(BandwidthTest, QueuesArePerDirectedLink) {
    // A->B busy does not delay B->A: each direction has its own queue.
    simulnetx::network net(0);
    auto& a = net.add_node("A");
    auto& b = net.add_node("B");
    net.link(a.id(), b.id(), {.bandwidth_bps = 1000});
    a.send(b.id(), bytes_of(1000));
    b.send(a.id(), bytes_of(1000));
    net.run_for(10s);
    EXPECT_EQ(delivered_times(net),
              (std::vector<std::chrono::nanoseconds>{8s, 8s}));
}

TEST(BandwidthTest, RelinkingKeepsTheQueue) {
    // Replacing a link's config does not reset its free_at: the queue
    // is link state, not config state.
    simulnetx::network net(0);
    auto& a = net.add_node("A");
    auto& b = net.add_node("B");
    net.link(a.id(), b.id(), {.bandwidth_bps = 1000});
    a.send(b.id(), bytes_of(1000));
    net.link(a.id(), b.id(), {.bandwidth_bps = 1000});
    a.send(b.id(), bytes_of(1000));
    net.run_for(20s);
    EXPECT_EQ(delivered_times(net),
              (std::vector<std::chrono::nanoseconds>{8s, 16s}));
}

TEST(BandwidthTest, PartitionMidTransmissionDropsButQueueAdvances) {
    // A message mid-transmission when a partition lands is dropped by
    // the delivery check (the partition must still be active at the
    // arrival time), but free_at still advances: the next message
    // queues behind the dropped one.
    simulnetx::network net(0);
    auto& a = net.add_node("A");
    auto& b = net.add_node("B");
    net.link(a.id(), b.id(), {.bandwidth_bps = 1000});
    a.send(b.id(), bytes_of(1000));
    a.after(1s, [&] { net.partition({a.id()}, {b.id()}); });
    a.after(9s, [&] { net.heal(); });
    net.run_for(9s);
    EXPECT_EQ(delivered_times(net).size(), 0U);
    EXPECT_EQ(static_cast<std::size_t>(net.dropped_count()), 1U);
    a.send(b.id(), bytes_of(1000));
    net.run_for(20s);
    EXPECT_EQ(delivered_times(net),
              (std::vector<std::chrono::nanoseconds>{17s}));
}

TEST(BandwidthTest, QueueSurvivesCrashRestart) {
    // free_at is link state, not node state: it survives crash/restart,
    // and a message in flight when the destination crashes is still
    // delivered if it arrives after the restart.
    simulnetx::network net(0);
    auto& a = net.add_node("A");
    auto& b = net.add_node("B");
    net.link(a.id(), b.id(), {.bandwidth_bps = 1000});
    a.send(b.id(), bytes_of(1000));
    a.after(1s, [&] { net.crash(b.id()); });
    a.after(2s, [&] { net.restart(b.id()); });
    net.run_for(9s);
    ASSERT_EQ(delivered_times(net).size(), 1U);
    EXPECT_EQ(delivered_times(net)[0], 8s);
    a.send(b.id(), bytes_of(1000));
    net.run_for(20s);
    EXPECT_EQ(delivered_times(net),
              (std::vector<std::chrono::nanoseconds>{8s, 17s}));
}

TEST(BandwidthTest, LossConsumesNoBandwidth) {
    // A lost message never enters the network: the next message does
    // not queue behind it.
    simulnetx::network net(0);
    auto& a = net.add_node("A");
    auto& b = net.add_node("B");
    net.link(a.id(), b.id(), {.loss_rate = 1.0, .bandwidth_bps = 1000});
    a.send(b.id(), bytes_of(1000));
    net.link(a.id(), b.id(), {.loss_rate = 0.0, .bandwidth_bps = 1000});
    a.send(b.id(), bytes_of(1000));
    net.run_for(10s);
    ASSERT_EQ(delivered_times(net).size(), 1U);
    EXPECT_EQ(delivered_times(net)[0], 8s);
}

TEST(BandwidthTest, ScenarioFlowsThrough) {
    // scenario::link takes a link_config, so bandwidth flows through
    // unchanged; three sends at t=0 arrive 8s/16s/24s apart.
    simulnetx::scenario s("bandwidth-scenario");
    s.seed(0).nodes("A", "B").link("A", "B", {.bandwidth_bps = 1000});
    s.send_at(0ns, "A", "B", std::vector<std::byte>(1000, std::byte{'x'}));
    s.send_at(0ns, "A", "B", std::vector<std::byte>(1000, std::byte{'x'}));
    s.send_at(0ns, "A", "B", std::vector<std::byte>(1000, std::byte{'x'}));
    s.run_until(30s);
    EXPECT_EQ(delivered_times(s.network()),
              (std::vector<std::chrono::nanoseconds>{8s, 16s, 24s}));
}

TEST(BandwidthTest, TypedSendCarriesSerializedSize) {
    // N is the bytes the link actually carries: for a typed send that
    // is the serialized form (8-byte tag + payload), not the user value.
    // A 4-byte int serializes to 12 bytes: 12 @ 1000 Bps = 96ms.
    simulnetx::network net(0);
    auto& a = net.add_node("A");
    auto& b = net.add_node("B");
    net.link(a.id(), b.id(), {.bandwidth_bps = 1000});
    int value = 42;
    a.send<int>(b.id(), value);
    net.run_for(1s);
    ASSERT_EQ(delivered_times(net).size(), 1U);
    EXPECT_EQ(delivered_times(net)[0], 96ms);
}

}  // namespace
