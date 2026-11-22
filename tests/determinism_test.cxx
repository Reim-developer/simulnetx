#include <chrono>
#include <cstddef>
#include <cstdint>
#include <vector>

#include <gtest/gtest.h>

#include "simulnetx/network.hxx"

namespace {

using namespace std::chrono_literals;

struct delivery {
    long long time_ns;
    simulnetx::node_id from;
    simulnetx::node_id to;
    int first_byte;
    bool operator==(const delivery&) const = default;
};

struct outcome {
    std::size_t sent;
    std::size_t delivered;
    std::size_t dropped;
    std::vector<delivery> log;
    bool operator==(const outcome&) const = default;
};

// One scenario exercising every sampling path plus timers and a partition.
outcome run_scenario(std::uint64_t seed) {
    simulnetx::network net(seed);
    auto& a = net.add_node("A");
    auto& b = net.add_node("B");
    auto& c = net.add_node("C");

    outcome result;
    const auto record = [&](const simulnetx::message& m) {
        result.log.push_back(
            delivery{net.now().count(), m.from, m.to, static_cast<int>(m.payload[0])});
    };
    a.on_message(record);
    b.on_message(record);
    c.on_message(record);

    net.link(a.id(), b.id(),
             {.latency = 10ms, .jitter = 5ms, .loss_rate = 0.2, .dup_rate = 0.1,
              .reorder_rate = 0.3});
    net.link(b.id(), c.id(),
             {.latency = 20ms, .jitter = 10ms, .loss_rate = 0.3, .dup_rate = 0.2,
              .reorder_rate = 0.2});
    net.link(a.id(), c.id(), {.latency = 5ms, .jitter = 2ms, .loss_rate = 0.1});

    for (int i = 0; i < 60; ++i) {
        a.send(b.id(), std::vector<std::byte>{static_cast<std::byte>(i)});
        b.send(c.id(), std::vector<std::byte>{static_cast<std::byte>(i)});
        c.send(a.id(), std::vector<std::byte>{static_cast<std::byte>(i)});
    }
    // A timer that sends mid-run.
    a.after(500ms, [&] { a.send(b.id(), "timer"); });
    // A partition in the middle, then a heal.
    b.after(300ms, [&] { net.partition({a.id()}, {b.id(), c.id()}); });
    b.after(700ms, [&] { net.heal(); });

    net.run_for(2s);
    result.sent = net.sent_count();
    result.delivered = net.delivered_count();
    result.dropped = net.dropped_count();
    return result;
}

TEST(DeterminismTest, SameSeedSameScenarioIsIdentical) {
    const outcome first = run_scenario(99);
    const outcome second = run_scenario(99);
    EXPECT_EQ(first, second);
    EXPECT_GT(first.log.size(), 0u);
    // The scenario is lossy, so some drops must have happened.
    EXPECT_GT(first.dropped, 0u);
}

TEST(DeterminismTest, DifferentSeedGivesDifferentOutcome) {
    const outcome a = run_scenario(99);
    const outcome b = run_scenario(100);
    EXPECT_NE(a, b);
}

}  // namespace
