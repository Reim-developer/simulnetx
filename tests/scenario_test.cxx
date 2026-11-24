#include <chrono>
#include <cstddef>
#include <stdexcept>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "simulnetx/scenario.hxx"

namespace {

using namespace std::chrono_literals;

// The agreed example chain, on a temporary, end to end. One scheduled ping
// is added: the chain as sketched never sends a message, so delivered would
// be 0 and the expect could not pass. (Verified: seed 42, 30% loss, the
// single ping survives.)
TEST(ScenarioTest, AgreedExampleChainEndToEnd) {
    simulnetx::scenario("retry under 30% loss")
        .seed(42)
        .nodes("A", "B")
        .link("A", "B", {.latency = 50ms, .loss_rate = 0.30})
        .send_at(0s, "A", "B", "ping")
        .partition_at(5s, {"A"}, {"B"})
        .heal_at(8s)
        .run_until(10s)
        .expect_delivered(1)
        .expect_sent(1)
        .expect_dropped(0);
}

// The same chain built step by step on a named object.
TEST(ScenarioTest, NamedObjectChaining) {
    simulnetx::scenario s("named");
    s.seed(3);
    s.nodes("A", "B");
    s.link("A", "B", {});
    s.send_at(0s, "A", "B", "x");
    s.run_until(1s);
    s.expect_delivered(1);
    EXPECT_EQ(s.name(), "named");
}

TEST(ScenarioTest, ExpectFailureMessageHasScenarioNameSeedAndTime) {
    simulnetx::scenario s("naming test");
    s.seed(42).nodes("A", "B").link("A", "B", {}).run_until(10s);
    try {
        s.expect_delivered(5);
        FAIL() << "expected assertion_failed";
    } catch (const simulnetx::assertion_failed& e) {
        const std::string msg = e.what();
        EXPECT_NE(msg.find("scenario 'naming test'"), std::string::npos);
        EXPECT_NE(msg.find("expected delivered_count == 5, got 0"), std::string::npos);
        EXPECT_NE(msg.find("seed 42"), std::string::npos);
        EXPECT_NE(msg.find("t=10000000000ns"), std::string::npos);
    }
}

TEST(ScenarioTest, PartitionAtDropsDuringWindowHealRestores) {
    simulnetx::scenario s("partition window");
    s.seed(1)
        .nodes("A", "B")
        .link("A", "B", {.latency = 500ms})
        .send_at(1s, "A", "B", "before")       // arrives 1.5s, delivered
        .send_at(1900ms, "A", "B", "in-flight")  // arrives 2.4s, dropped at delivery
        .send_at(3s, "A", "B", "during")       // dropped at send (partitioned)
        .send_at(5s, "A", "B", "after")        // arrives 5.5s, delivered
        .partition_at(2s, {"A"}, {"B"})
        .heal_at(4s)
        .run_until(6s)
        .expect_sent(4)
        .expect_delivered(2)
        .expect_dropped(2);
}

TEST(ScenarioTest, UnknownNameThrows) {
    simulnetx::scenario s("unknown names");
    s.nodes("A", "B");
    EXPECT_THROW(s.link("A", "C", {}), std::invalid_argument);
    EXPECT_THROW(s.link("C", "A", {}), std::invalid_argument);
    EXPECT_THROW(s.on_message("C", [](const simulnetx::message&) {}), std::invalid_argument);
    EXPECT_THROW(s.node("C"), std::invalid_argument);
    EXPECT_THROW(s.send_at(0s, "A", "C", "x"), std::invalid_argument);
    EXPECT_THROW(s.send_at(0s, "C", "B", "x"), std::invalid_argument);
    EXPECT_THROW(s.partition_at(1s, {"A"}, {"C"}), std::invalid_argument);
    EXPECT_THROW(s.partition_at(1s, {"C"}, {"B"}), std::invalid_argument);
    // A name in both groups is also rejected, at schedule time.
    EXPECT_THROW(s.partition_at(1s, {"A"}, {"A"}), std::invalid_argument);
}

TEST(ScenarioTest, DuplicateNodeNameThrows) {
    simulnetx::scenario s("dup");
    EXPECT_THROW(s.nodes("A", "A"), std::invalid_argument);
    simulnetx::scenario t("dup2");
    t.nodes("A");
    EXPECT_THROW(t.nodes("A"), std::invalid_argument);
}

TEST(ScenarioTest, LateSeedThrowsLogicError) {
    simulnetx::scenario s("late seed");
    s.nodes("A");
    EXPECT_THROW(s.seed(7), std::logic_error);
    simulnetx::scenario t("late seed 2");
    t.nodes("A", "B").link("A", "B", {}).run_until(1s);
    EXPECT_THROW(t.seed(7), std::logic_error);
}

TEST(ScenarioTest, InvalidLinkConfigThrows) {
    simulnetx::scenario s("bad link");
    s.nodes("A", "B");
    EXPECT_THROW(s.link("A", "B", {.loss_rate = 1.5}), std::invalid_argument);
    EXPECT_THROW(s.link("A", "B", {.latency = -1ms}), std::invalid_argument);
    EXPECT_THROW(s.link("A", "A", {}), std::invalid_argument);
}

TEST(ScenarioTest, NodeAccessorHandlerAndSend) {
    simulnetx::scenario s("escape hatch");
    s.nodes("A", "B").link("A", "B", {});
    std::string seen;
    s.node("B").on_message([&](const simulnetx::message& m) {
        seen.assign(reinterpret_cast<const char*>(m.payload.data()), m.payload.size());
    });
    s.node("A").send(s.node("B").id(), "direct");
    s.run_until(1s);
    EXPECT_EQ(seen, "direct");
    s.expect_delivered(1);
}

TEST(ScenarioTest, OnMessageSetsHandler) {
    simulnetx::scenario s("on_message");
    std::string seen;
    s.nodes("A", "B")
        .link("A", "B", {})
        .on_message("B", [&](const simulnetx::message& m) {
            seen.assign(reinterpret_cast<const char*>(m.payload.data()), m.payload.size());
        })
        .send_at(0s, "A", "B", "via scenario")
        .run_until(1s);
    EXPECT_EQ(seen, "via scenario");
}

TEST(ScenarioTest, SendAtDeliversAtScheduledTime) {
    simulnetx::scenario s("send_at timing");
    s.nodes("A", "B").link("A", "B", {});
    long long arrived_at = -1;
    s.on_message("B", [&](const simulnetx::message&) {
        arrived_at = s.network().now().count();
    });
    const std::vector<std::byte> payload{std::byte{0x01}, std::byte{0x02}};
    s.send_at(100ms, "A", "B", payload).run_until(1s);
    EXPECT_EQ(arrived_at, 100000000LL);
    s.expect_delivered(1);
}

TEST(ScenarioTest, ScheduleInPastThrows) {
    simulnetx::scenario s("past");
    s.nodes("A", "B").link("A", "B", {}).run_until(10s);
    EXPECT_THROW(s.partition_at(5s, {"A"}, {"B"}), std::invalid_argument);
    EXPECT_THROW(s.heal_at(5s), std::invalid_argument);
    EXPECT_THROW(s.send_at(5s, "A", "B", "x"), std::invalid_argument);
    EXPECT_THROW(s.crash_at(5s, "A"), std::invalid_argument);
    EXPECT_THROW(s.restart_at(5s, "A"), std::invalid_argument);
}

TEST(ScenarioTest, HealAtWithoutPartitionIsHarmless) {
    simulnetx::scenario s("heal no-op");
    s.nodes("A", "B")
        .link("A", "B", {})
        .send_at(0s, "A", "B", "x")
        .heal_at(500ms)
        .run_until(1s)
        .expect_delivered(1)
        .expect_dropped(0);
}

TEST(ScenarioTest, ExpectBetweenPassesAndFails) {
    simulnetx::scenario s("between");
    s.nodes("A", "B")
        .link("A", "B", {})
        .send_at(0s, "A", "B", "x")
        .send_at(0s, "A", "B", "y")
        .run_until(1s);
    s.expect_delivered_between(1, 3);
    s.expect_sent_between(2, 2);
    s.expect_dropped_between(0, 0);
    EXPECT_THROW(s.expect_delivered_between(5, 9), simulnetx::assertion_failed);
    EXPECT_THROW(s.expect_dropped_between(1, 2), simulnetx::assertion_failed);
}

// One lossy run, built the same way twice: identical stats and identical
// delivery-time log.
void build_lossy(simulnetx::scenario& s, std::vector<long long>& arrivals) {
    s.seed(7)
        .nodes("A", "B")
        .link("A", "B", {.latency = 10ms, .jitter = 5ms, .loss_rate = 0.2, .dup_rate = 0.1,
                         .reorder_rate = 0.3})
        .on_message("B", [&](const simulnetx::message&) {
            arrivals.push_back(s.network().now().count());
        });
    for (int i = 0; i < 50; ++i) {
        s.send_at(std::chrono::milliseconds(i * 10), "A", "B", "x");
    }
    s.run_until(10s);
}

TEST(ScenarioTest, DeterminismSameScenarioTwice) {
    std::vector<long long> first_arrivals;
    std::vector<long long> second_arrivals;
    simulnetx::scenario first("determinism");
    build_lossy(first, first_arrivals);
    simulnetx::scenario second("determinism");
    build_lossy(second, second_arrivals);

    EXPECT_EQ(static_cast<std::size_t>(first.network().sent_count()),
              static_cast<std::size_t>(second.network().sent_count()));
    EXPECT_EQ(static_cast<std::size_t>(first.network().delivered_count()),
              static_cast<std::size_t>(second.network().delivered_count()));
    EXPECT_EQ(static_cast<std::size_t>(first.network().dropped_count()),
              static_cast<std::size_t>(second.network().dropped_count()));
    EXPECT_EQ(first_arrivals, second_arrivals);
    EXPECT_GT(first_arrivals.size(), 0u);
}

TEST(ScenarioTest, DefaultSeedIsZero) {
    // A scenario built without seed() behaves exactly like seed(0).
    std::vector<long long> unseeded;
    std::vector<long long> seeded_zero;
    simulnetx::scenario u("unseeded");
    u.nodes("A", "B")
        .link("A", "B", {.latency = 10ms, .jitter = 5ms, .loss_rate = 0.2, .dup_rate = 0.1,
                         .reorder_rate = 0.3})
        .on_message("B",
                    [&](const simulnetx::message&) { unseeded.push_back(u.network().now().count()); });
    for (int i = 0; i < 50; ++i) {
        u.send_at(std::chrono::milliseconds(i * 10), "A", "B", "x");
    }
    u.run_until(10s);
    simulnetx::scenario z("seed zero");
    z.seed(0)
        .nodes("A", "B")
        .link("A", "B", {.latency = 10ms, .jitter = 5ms, .loss_rate = 0.2, .dup_rate = 0.1,
                         .reorder_rate = 0.3})
        .on_message("B", [&](const simulnetx::message&) {
            seeded_zero.push_back(z.network().now().count());
        });
    for (int i = 0; i < 50; ++i) {
        z.send_at(std::chrono::milliseconds(i * 10), "A", "B", "x");
    }
    z.run_until(10s);
    EXPECT_EQ(unseeded, seeded_zero);
    EXPECT_EQ(static_cast<std::size_t>(u.network().delivered_count()),
              static_cast<std::size_t>(z.network().delivered_count()));
    EXPECT_GT(unseeded.size(), 0u);
}

TEST(ScenarioTest, CrashAtRestartAtEndToEnd) {
    simulnetx::scenario s("crash window");
    s.seed(1)
        .nodes("A", "B")
        .link("A", "B", {.latency = 500ms})
        .send_at(0s, "A", "B", "before")      // arrives 0.5s, delivered
        .send_at(900ms, "A", "B", "in-flight")  // arrives 1.4s, dropped at delivery
        .send_at(2s, "A", "B", "during")      // dropped at send (B crashed)
        .send_at(4s, "A", "B", "after")       // arrives 4.5s, delivered
        .crash_at(1s, "B")
        .restart_at(3s, "B")
        .run_until(6s)
        .expect_sent(4)
        .expect_delivered(2)
        .expect_dropped(2);
    EXPECT_FALSE(s.network().is_crashed(s.node("B").id()));
}

TEST(ScenarioTest, CrashAtRestartAtUnknownNameThrows) {
    simulnetx::scenario s("unknown crash");
    s.nodes("A", "B");
    EXPECT_THROW(s.crash_at(1s, "C"), std::invalid_argument);
    EXPECT_THROW(s.restart_at(1s, "C"), std::invalid_argument);
}

TEST(ScenarioTest, SchedulingWithZeroNodesWorks) {
    // v0.3.0 behavior change: scheduling used to go through a node's timer
    // and threw std::logic_error before any node existed. Scheduling now
    // uses network-level timers, so it works on an empty scenario.
    simulnetx::scenario s("zero nodes");
    EXPECT_NO_THROW(s.heal_at(1s));
    EXPECT_NO_THROW(s.partition_at(1s, {}, {}));
    EXPECT_NO_THROW(s.run_until(2s));
    s.expect_sent(0).expect_delivered(0).expect_dropped(0);
    // Unknown names still throw, even with zero nodes.
    EXPECT_THROW(s.crash_at(1s, "A"), std::invalid_argument);
    EXPECT_THROW(s.send_at(1s, "A", "B", "x"), std::invalid_argument);
}

TEST(ScenarioTest, LinkDirectedChain) {
    simulnetx::scenario s("directed");
    s.nodes("A", "B")
        .link_directed("A", "B", {})
        .send_at(0s, "A", "B", "x")
        .run_until(1s)
        .expect_delivered(1);
    EXPECT_THROW(s.link_directed("A", "C", {}), std::invalid_argument);
    EXPECT_THROW(s.link_directed("A", "A", {}), std::invalid_argument);
}

TEST(ScenarioTest, SendAtAgainstMissingDirectedLinkThrowsInRunUntil) {
    simulnetx::scenario s("no reverse link");
    s.nodes("A", "B").link_directed("A", "B", {}).send_at(0s, "B", "A", "x");
    EXPECT_THROW(s.run_until(1s), std::invalid_argument);
}

TEST(ScenarioTest, SendAtWhileSenderCrashedThrowsInRunUntil) {
    // Loud, not silent: the scenario is inconsistent and the user must see it.
    simulnetx::scenario s("crashed sender");
    s.nodes("A", "B")
        .link("A", "B", {})
        .crash_at(1s, "A")
        .send_at(2s, "A", "B", "x");
    EXPECT_THROW(s.run_until(5s), std::logic_error);
}

TEST(ScenarioTest, ExpectEventsPassesAndFails) {
    simulnetx::scenario s("trace expects");
    s.nodes("A", "B")
        .link("A", "B", {})
        .send_at(0s, "A", "B", "x")
        .send_at(0s, "A", "B", "y")
        .run_until(1s);
    s.expect_events(simulnetx::event_kind::sent, 2);
    s.expect_events(simulnetx::event_kind::delivered, 2);
    s.expect_events(simulnetx::event_kind::dropped, 0);
    s.expect_events_between(simulnetx::event_kind::sent, 1, 3);
    s.expect_events_between(simulnetx::event_kind::timer_fired, 2, 2);
    EXPECT_THROW(s.expect_events(simulnetx::event_kind::sent, 5),
                 simulnetx::assertion_failed);
    EXPECT_THROW(s.expect_events_between(simulnetx::event_kind::dropped, 1, 2),
                 simulnetx::assertion_failed);
}

TEST(ScenarioTest, ExpectEventsFailureHasScenarioNameSeedAndTime) {
    simulnetx::scenario s("naming events");
    s.seed(42).nodes("A", "B").link("A", "B", {}).run_until(10s);
    try {
        s.expect_events(simulnetx::event_kind::sent, 5);
        FAIL() << "expected assertion_failed";
    } catch (const simulnetx::assertion_failed& e) {
        const std::string msg = e.what();
        EXPECT_NE(msg.find("scenario 'naming events'"), std::string::npos);
        EXPECT_NE(msg.find("expected event_count(sent) == 5, got 0"), std::string::npos);
        EXPECT_NE(msg.find("seed 42"), std::string::npos);
        EXPECT_NE(msg.find("t=10000000000ns"), std::string::npos);
    }
}

TEST(ScenarioTest, TraceAndDumpPassthrough) {
    simulnetx::scenario s("passthrough");
    s.seed(42).nodes("A", "B").link("A", "B", {.latency = 50ms});
    s.send_at(0s, "A", "B", "hi");
    s.run_until(1s);
    // One send_at timer + one send + one delivery.
    EXPECT_EQ(s.trace().size(), 3U);
    EXPECT_EQ(s.dump_trace(),
              "trace: 3 events (seed 42)\n"
              "t=0ns timer #0 fired [network]\n"
              "t=0ns A -> B sent 2 bytes\n"
              "t=50000000ns A -> B delivered 2 bytes\n");
}

}  // namespace
