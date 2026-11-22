// Golden values: exact outputs pinned for fixed seeds. These guard against
// RNG drift - if the RNG, the draw order, or the sampling math changes, these
// tests fail and the new values must be consciously re-pinned (and the change
// documented as breaking determinism for existing scenarios).

#include <chrono>
#include <vector>

#include <gtest/gtest.h>

#include "simulnetx/network.hxx"

namespace {

using namespace std::chrono_literals;

TEST(GoldenTest, LossSeed7) {
    simulnetx::network net(7);
    auto& s = net.add_node("S");
    auto& r = net.add_node("R");
    net.link(s.id(), r.id(), {.loss_rate = 0.5});
    for (int i = 0; i < 200; ++i) {
        s.send(r.id(), "p");
    }
    net.run_for(1s);
    net.sent_count().equal(200);
    net.dropped_count().equal(94);
    net.delivered_count().equal(106);
}

TEST(GoldenTest, DupSeed11) {
    simulnetx::network net(11);
    auto& s = net.add_node("S");
    auto& r = net.add_node("R");
    net.link(s.id(), r.id(), {.dup_rate = 0.5});
    for (int i = 0; i < 200; ++i) {
        s.send(r.id(), "p");
    }
    net.run_for(1s);
    net.sent_count().equal(200);
    net.delivered_count().equal(294);
    net.dropped_count().equal(0);
}

TEST(GoldenTest, ReorderSeed13) {
    simulnetx::network net(13);
    auto& s = net.add_node("S");
    auto& r = net.add_node("R");
    std::vector<int> order;
    r.on_message(
        [&](const simulnetx::message& m) { order.push_back(static_cast<int>(m.payload[0])); });
    net.link(s.id(), r.id(), {.jitter = 10ms, .reorder_rate = 0.5});
    for (int i = 0; i < 50; ++i) {
        s.send(r.id(), std::vector<std::byte>{static_cast<std::byte>(i)});
    }
    net.run_for(1s);
    const std::vector<int> expected = {14, 44, 35, 33, 17, 18, 13, 28, 32, 9, 1, 34, 31,
                                       2,  26, 12, 46, 5,  16, 22, 29, 30, 4,  10, 45, 37,
                                       36, 47, 24, 15, 21, 6,  23, 19, 49, 40, 48, 38, 42,
                                       3,  11, 20, 7,  0,  8,  27, 43, 41, 25, 39};
    EXPECT_EQ(order, expected);
    int out_of_order = 0;
    for (std::size_t i = 1; i < order.size(); ++i) {
        if (order[i] < order[i - 1]) {
            ++out_of_order;
        }
    }
    EXPECT_EQ(out_of_order, 24);
}

TEST(GoldenTest, JitterSeed17) {
    simulnetx::network net(17);
    auto& s = net.add_node("S");
    auto& r = net.add_node("R");
    long long sum = 0;
    long long mn = 1LL << 60;
    long long mx = 0;
    r.on_message([&](const simulnetx::message&) {
        const long long t = net.now().count();
        sum += t;
        if (t < mn) {
            mn = t;
        }
        if (t > mx) {
            mx = t;
        }
    });
    net.link(s.id(), r.id(), {.jitter = 1000ns});
    for (int i = 0; i < 100; ++i) {
        s.send(r.id(), "p");
    }
    net.run_for(1s);
    EXPECT_EQ(sum, 47323LL);
    EXPECT_EQ(mn, 11LL);
    EXPECT_EQ(mx, 998LL);
}

}  // namespace
