#include <chrono>
#include <cstddef>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "simulnetx/errors.hxx"
#include "simulnetx/message.hxx"
#include "simulnetx/network.hxx"
#include "simulnetx/scenario.hxx"
#include "simulnetx/serializer.hxx"

namespace {

using namespace std::chrono_literals;
using simulnetx::decode_error;
using simulnetx::message;
using simulnetx::network;
using simulnetx::scenario;
using simulnetx::unhandled_message;

struct hello {
    std::string name;
    bool operator==(const hello&) const = default;
};

struct world {
    std::string text;
    int code;
    bool operator==(const world&) const = default;
};

TEST(TypedTest, TypedSendReachesTypedHandler) {
    network net{42};
    auto& a = net.add_node("A");
    auto& b = net.add_node("B");
    net.link(a.id(), b.id(), {.latency = 100ms});

    std::vector<hello> received;
    b.on<hello>([&received](const hello& h) { received.push_back(h); });

    a.send<hello>(b.id(), hello{"reim"});
    net.run_until(1s);

    ASSERT_EQ(received.size(), 1u);
    EXPECT_EQ(received[0], (hello{"reim"}));
    EXPECT_EQ(net.delivered_count(), 1u);
}

TEST(TypedTest, MultipleTypedHandlersEachGetTheirOwnType) {
    network net{42};
    auto& a = net.add_node("A");
    auto& b = net.add_node("B");
    net.link(a.id(), b.id(), {});

    std::vector<hello> hellos;
    std::vector<world> worlds;
    b.on<hello>([&hellos](const hello& h) { hellos.push_back(h); });
    b.on<world>([&worlds](const world& w) { worlds.push_back(w); });

    a.send<hello>(b.id(), hello{"one"});
    a.send<world>(b.id(), world{"two", 2});
    a.send<hello>(b.id(), hello{"three"});
    net.run_until(1s);

    EXPECT_EQ(hellos, (std::vector<hello>{{"one"}, {"three"}}));
    EXPECT_EQ(worlds, (std::vector<world>{{"two", 2}}));
}

TEST(TypedTest, RegisteringAgainReplacesTheHandler) {
    network net{42};
    auto& a = net.add_node("A");
    auto& b = net.add_node("B");
    net.link(a.id(), b.id(), {});

    int first_calls = 0;
    int second_calls = 0;
    b.on<hello>([&first_calls](const hello&) { ++first_calls; });
    b.on<hello>([&second_calls](const hello&) { ++second_calls; });

    a.send<hello>(b.id(), hello{"x"});
    net.run_until(1s);

    EXPECT_EQ(first_calls, 0);
    EXPECT_EQ(second_calls, 1);
}

TEST(TypedTest, UnmatchedFallsThroughToOnMessage) {
    network net{42};
    auto& a = net.add_node("A");
    auto& b = net.add_node("B");
    net.link(a.id(), b.id(), {});

    int hello_calls = 0;
    std::vector<world> raw_worlds;
    b.on<hello>([&hello_calls](const hello&) { ++hello_calls; });
    b.on_message([&raw_worlds](const message& msg) {
        raw_worlds.push_back(msg.as<world>());
    });

    a.send<hello>(b.id(), hello{"typed"});
    a.send<world>(b.id(), world{"raw", 9});
    net.run_until(1s);

    EXPECT_EQ(hello_calls, 1);
    EXPECT_EQ(raw_worlds, (std::vector<world>{{"raw", 9}}));
}

TEST(TypedTest, StrictModeThrowsOnUnmatchedTag) {
    network net{42};
    auto& a = net.add_node("A");
    auto& b = net.add_node("B");
    net.link(a.id(), b.id(), {.latency = 100ms});
    EXPECT_TRUE(net.strict_tags());

    b.on<hello>([](const hello&) { FAIL() << "hello handler must not fire"; });
    a.send<world>(b.id(), world{"nobody handles me", 1});

    try {
        net.run_until(1s);
        FAIL() << "expected unhandled_message";
    } catch (const unhandled_message& e) {
        const std::string what = e.what();
        EXPECT_NE(what.find("'B'"), std::string::npos);
        EXPECT_NE(what.find("seed 42"), std::string::npos);
        EXPECT_NE(what.find("t=100000000ns"), std::string::npos);
        EXPECT_NE(what.find("tag "), std::string::npos);
    }
    // The message still counts as delivered: it arrived, nobody handled it.
    EXPECT_EQ(net.delivered_count(), 1u);
}

TEST(TypedTest, StrictModeThrowsOnShortUntaggedMessage) {
    network net{42};
    auto& a = net.add_node("A");
    auto& b = net.add_node("B");
    net.link(a.id(), b.id(), {});

    b.on<hello>([](const hello&) { FAIL() << "hello handler must not fire"; });
    a.send(b.id(), "ping");  // raw, 4 bytes: too short for a tag
    EXPECT_THROW(net.run_until(1s), unhandled_message);
}

TEST(TypedTest, LenientModeIgnoresUnmatchedTag) {
    network net{42};
    net.set_strict_tags(false);
    auto& a = net.add_node("A");
    auto& b = net.add_node("B");
    net.link(a.id(), b.id(), {});

    b.on<hello>([](const hello&) { FAIL() << "hello handler must not fire"; });
    a.send<world>(b.id(), world{"ignored", 1});
    EXPECT_NO_THROW(net.run_until(1s));
    EXPECT_EQ(net.delivered_count(), 1u);
}

TEST(TypedTest, NodeWithNoHandlersIgnoresTypedMessages) {
    network net{42};
    auto& a = net.add_node("A");
    auto& b = net.add_node("B");
    net.link(a.id(), b.id(), {});

    // B has no handler at all: the v0.1.0 behavior, even in strict mode.
    a.send<hello>(b.id(), hello{"nobody home"});
    EXPECT_NO_THROW(net.run_until(1s));
    EXPECT_EQ(net.delivered_count(), 1u);
}

TEST(TypedTest, RawOnlyNodeIsUnaffectedByStrictMode) {
    network net{42};
    auto& a = net.add_node("A");
    auto& b = net.add_node("B");
    net.link(a.id(), b.id(), {});

    int raw_calls = 0;
    b.on_message([&raw_calls](const message&) { ++raw_calls; });
    a.send<hello>(b.id(), hello{"raw sees me"});
    EXPECT_NO_THROW(net.run_until(1s));
    EXPECT_EQ(raw_calls, 1);
}

TEST(TypedTest, AsWithWrongTypeThrowsDecodeError) {
    network net{42};
    auto& a = net.add_node("A");
    auto& b = net.add_node("B");
    net.link(a.id(), b.id(), {});

    bool threw = false;
    b.on_message([&threw](const message& msg) {
        try {
            (void)msg.as<world>();
        } catch (const decode_error&) {
            threw = true;
        }
    });
    a.send<hello>(b.id(), hello{"not a world"});
    net.run_until(1s);
    EXPECT_TRUE(threw);
}

TEST(TypedTest, AsOnRawMessageThrowsDecodeError) {
    network net{42};
    auto& a = net.add_node("A");
    auto& b = net.add_node("B");
    net.link(a.id(), b.id(), {});

    int throws = 0;
    b.on_message([&throws](const message& msg) {
        try {
            (void)msg.as<hello>();
        } catch (const decode_error&) {
            ++throws;
        }
    });
    a.send(b.id(), "ping");               // too short for a tag
    a.send(b.id(), "a longer raw payload");  // long enough, wrong tag
    net.run_until(1s);
    EXPECT_EQ(throws, 2);
}

TEST(TypedTest, AsWithTrailingBytesThrowsDecodeError) {
    network net{42};
    auto& a = net.add_node("A");
    auto& b = net.add_node("B");
    net.link(a.id(), b.id(), {});

    bool threw = false;
    b.on_message([&threw](const message& msg) {
        try {
            (void)msg.as<hello>();
        } catch (const decode_error&) {
            threw = true;
        }
    });

    // A well-formed typed payload plus one junk byte.
    std::vector<std::byte> payload;
    simulnetx::detail::append_u64(payload, simulnetx::detail::type_tag<hello>());
    simulnetx::serializer<hello>::encode(hello{"reim"}, payload);
    payload.push_back(std::byte{0});
    a.send(b.id(), payload);

    net.run_until(1s);
    EXPECT_TRUE(threw);
}

TEST(TypedTest, TypedSendToCrashedNodeIsDropped) {
    network net{42};
    auto& a = net.add_node("A");
    auto& b = net.add_node("B");
    net.link(a.id(), b.id(), {});

    b.on<hello>([](const hello&) { FAIL() << "crashed node must not receive"; });
    net.crash(b.id());
    a.send<hello>(b.id(), hello{"lost"});
    net.run_until(1s);

    EXPECT_EQ(net.dropped_count(), 1u);
    EXPECT_EQ(net.delivered_count(), 0u);
}

TEST(TypedTest, TypedSendWithoutLinkThrows) {
    network net{42};
    auto& a = net.add_node("A");
    auto& b = net.add_node("B");
    EXPECT_THROW(a.send<hello>(b.id(), hello{"no link"}), std::invalid_argument);
}

TEST(TypedTest, TypedSelfSendDeliversImmediately) {
    network net{42};
    auto& a = net.add_node("A");

    std::vector<hello> received;
    a.on<hello>([&received](const hello& h) { received.push_back(h); });
    a.send<hello>(a.id(), hello{"self"});
    net.run_until(0s);

    EXPECT_EQ(received, (std::vector<hello>{{"self"}}));
    EXPECT_EQ(net.now(), 0s);
}

TEST(TypedTest, TypedDeliveryOrderIsDeterministic) {
    const auto run_once = [] {
        network net{7};
        auto& a = net.add_node("A");
        auto& b = net.add_node("B");
        net.link(a.id(), b.id(), {.latency = 10ms, .loss_rate = 0.3});
        std::vector<int> received;
        b.on<world>([&received](const world& w) { received.push_back(w.code); });
        for (int i = 0; i < 20; ++i) {
            a.send<world>(b.id(), world{"w", i});
        }
        net.run_until(1s);
        return received;
    };
    const std::vector<int> first = run_once();
    const std::vector<int> second = run_once();
    EXPECT_EQ(first, second);
    EXPECT_FALSE(first.empty());
}

TEST(TypedTest, ScenarioLenientTagsIgnoresUnmatched) {
    scenario sc{"lenient"};
    sc.seed(42).nodes("A", "B").link("A", "B", {});
    sc.node("B").on<hello>([](const hello&) { FAIL() << "hello handler must not fire"; });
    sc.node("A").send(sc.node("B").id(), world{"ignored", 1});
    EXPECT_NO_THROW(sc.lenient_tags().run_until(1s));
    sc.expect_delivered(1);
}

TEST(TypedTest, ScenarioStrictIsTheDefault) {
    scenario sc{"strict"};
    sc.seed(42).nodes("A", "B").link("A", "B", {});
    sc.node("B").on<hello>([](const hello&) { FAIL() << "hello handler must not fire"; });
    sc.node("A").send(sc.node("B").id(), world{"unmatched", 1});
    EXPECT_THROW(sc.run_until(1s), unhandled_message);
}

TEST(TypedTest, ScenarioStrictFlagCanBeSetBeforeTheNetworkExists) {
    scenario sc{"order"};
    sc.lenient_tags().seed(42).nodes("A");
    EXPECT_FALSE(sc.network().strict_tags());

    scenario sc2{"order2"};
    sc2.seed(42).nodes("A");
    EXPECT_TRUE(sc2.network().strict_tags());
    sc2.lenient_tags();
    EXPECT_FALSE(sc2.network().strict_tags());
    sc2.strict_tags();
    EXPECT_TRUE(sc2.network().strict_tags());
}

}  // namespace
