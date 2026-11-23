#include <chrono>
#include <vector>

#include <gtest/gtest.h>

#include "simulnetx/scenario.hxx"

namespace {

using namespace std::chrono_literals;
using simulnetx::scenario;

// Send from every node to every other node; returns the delivered count
// after running to 1s. A missing link throws out of send, so a fully
// linked set of n nodes delivers exactly n * (n - 1) messages.
std::size_t all_to_all(scenario& sc, const std::vector<const char*>& names) {
    for (std::size_t i = 0; i < names.size(); ++i) {
        for (std::size_t j = 0; j < names.size(); ++j) {
            if (i != j) {
                sc.node(names[i]).send(sc.node(names[j]).id(), "ping");
            }
        }
    }
    sc.run_until(1s);
    return static_cast<std::size_t>(sc.network().delivered_count());
}

TEST(TopologyTest, MeshLinksAllPairs) {
    scenario sc{"mesh"};
    sc.seed(42).nodes("A", "B", "C", "D").mesh({});
    EXPECT_EQ(all_to_all(sc, {"A", "B", "C", "D"}), 12u);
}

TEST(TopologyTest, MeshSubsetLinksOnlyTheNamedNodes) {
    scenario sc{"mesh subset"};
    sc.seed(42).nodes("A", "B", "C", "D").mesh({"A", "B", "C"}, {});
    EXPECT_EQ(all_to_all(sc, {"A", "B", "C"}), 6u);
    // D is not linked to anyone.
    EXPECT_THROW(sc.node("A").send(sc.node("D").id(), "ping"), std::invalid_argument);
    EXPECT_THROW(sc.node("D").send(sc.node("A").id(), "ping"), std::invalid_argument);
}

TEST(TopologyTest, MeshWithUnknownNameThrows) {
    scenario sc{"mesh unknown"};
    sc.seed(42).nodes("A", "B");
    EXPECT_THROW(sc.mesh({"A", "Z"}, {}), std::invalid_argument);
}

TEST(TopologyTest, MeshTooFewNodesThrows) {
    scenario sc{"mesh one"};
    sc.seed(42).nodes("A");
    EXPECT_THROW(sc.mesh({}), std::logic_error);

    scenario sc2{"mesh none"};
    EXPECT_THROW(sc2.mesh({}), std::logic_error);
    EXPECT_THROW(sc2.mesh({"A"}, {}), std::logic_error);
}

TEST(TopologyTest, StarLinksHubToEveryOtherNode) {
    scenario sc{"star"};
    sc.seed(42).nodes("A", "B", "C", "D").star("B", {});
    // The hub reaches everyone, and everyone reaches the hub.
    EXPECT_EQ(all_to_all(sc, {"A", "B"}), 2u);
    // Spokes are not linked to each other.
    EXPECT_THROW(sc.node("A").send(sc.node("C").id(), "ping"), std::invalid_argument);
    EXPECT_THROW(sc.node("C").send(sc.node("D").id(), "ping"), std::invalid_argument);
}

TEST(TopologyTest, StarUnknownHubThrows) {
    scenario sc{"star unknown"};
    sc.seed(42).nodes("A", "B");
    EXPECT_THROW(sc.star("Z", {}), std::invalid_argument);
}

TEST(TopologyTest, StarTooFewNodesThrows) {
    scenario sc{"star one"};
    sc.seed(42).nodes("A");
    EXPECT_THROW(sc.star("A", {}), std::logic_error);
}

TEST(TopologyTest, ChainLinksConsecutiveNodesInInsertionOrder) {
    scenario sc{"chain"};
    // Insertion order is C, A, B - deliberately NOT alphabetical, so this
    // test proves chain() follows insertion order: C <-> A <-> B.
    sc.seed(42).nodes("C", "A", "B").chain({});
    sc.node("C").send(sc.node("A").id(), "ping");
    sc.node("A").send(sc.node("B").id(), "ping");
    sc.node("B").send(sc.node("A").id(), "ping");
    sc.node("A").send(sc.node("C").id(), "ping");
    sc.run_until(1s);
    EXPECT_EQ(static_cast<std::size_t>(sc.network().delivered_count()), 4u);
    // C and B are not directly linked.
    EXPECT_THROW(sc.node("C").send(sc.node("B").id(), "ping"), std::invalid_argument);
}

TEST(TopologyTest, ChainTooFewNodesThrows) {
    scenario sc{"chain one"};
    sc.seed(42).nodes("A");
    EXPECT_THROW(sc.chain({}), std::logic_error);
}

TEST(TopologyTest, MeshAfterAddingNodesLinksTheNewNodeToo) {
    scenario sc{"mesh twice"};
    sc.seed(42).nodes("A", "B").mesh({});
    sc.nodes("C").mesh({});
    EXPECT_EQ(all_to_all(sc, {"A", "B", "C"}), 6u);
}

TEST(TopologyTest, TopologyAppliesTheLinkConfig) {
    scenario sc{"mesh latency"};
    sc.seed(42).nodes("A", "B").mesh({.latency = 100ms});
    sc.node("A").send(sc.node("B").id(), "ping");
    sc.run_until(50ms);
    EXPECT_EQ(static_cast<std::size_t>(sc.network().delivered_count()), 0u);
    sc.run_until(100ms);
    EXPECT_EQ(static_cast<std::size_t>(sc.network().delivered_count()), 1u);
}

}  // namespace
