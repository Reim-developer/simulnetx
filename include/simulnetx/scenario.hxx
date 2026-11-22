#pragma once

#include <chrono>
#include <concepts>
#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <string>
#include <string_view>
#include <utility>

#include "simulnetx/link_config.hxx"
#include "simulnetx/network.hxx"
#include "simulnetx/node.hxx"

namespace simulnetx {

// A fluent, declarative facade over network for writing scenarios.
//
// A scenario is a named, single-use description of one simulation run: seed,
// nodes, links, scheduled faults, then run and check. Every chaining method
// returns scenario&, so a whole test reads as one declaration:
//
//   scenario("retry under 30% loss")
//       .seed(42)
//       .nodes("A", "B")
//       .link("A", "B", {.latency = 50ms, .loss_rate = 0.30})
//       .partition_at(5s, {"A"}, {"B"})
//       .heal_at(8s)
//       .run_until(10s)
//       .expect_delivered(1);
//
// This is a facade, not a test framework: there are no test blocks, no
// runner, no fixtures. The expect_* checks exist only because they carry
// simulation context (the seed and the virtual time) that a general test
// framework cannot provide; use them inside whatever test runner you like.
//
// Execution is IMMEDIATE: each chained method acts when called, in chain
// order. partition_at/heal_at/send_at schedule a timer now; run_until runs
// the network now. Schedule-then-run ordering is the caller's
// responsibility: scheduling an event in the past (after run_until has
// advanced beyond it) throws std::invalid_argument.
//
// Like network, a scenario is single-owner: it cannot be copied or moved.
// Chain on a temporary or on a named object; both work.
class scenario {
public:
    explicit scenario(std::string name) : name_(std::move(name)) {}

    scenario(const scenario&) = delete;
    scenario& operator=(const scenario&) = delete;
    scenario(scenario&&) = delete;
    scenario& operator=(scenario&&) = delete;

    const std::string& name() const { return name_; }

    // Set the seed for the run. Must be called before the network is created
    // (any nodes/link/run call creates it); afterwards throws
    // std::logic_error. The default seed, when never set, is 0.
    scenario& seed(std::uint64_t s);

    // Create nodes by name. A duplicate name throws std::invalid_argument.
    template <typename... Names>
        requires(std::convertible_to<Names, std::string_view> && ...)
    scenario& nodes(Names&&... names) {
        (add_node_named(std::string_view(names)), ...);
        return *this;
    }

    // Link two nodes by name, with the same config and validation as
    // network::link. An unknown name throws std::invalid_argument.
    scenario& link(std::string_view a, std::string_view b, link_config cfg);

    // Escape hatch: the node with this name, for handlers, timers and direct
    // sends. An unknown name throws std::invalid_argument.
    simulnetx::node& node(std::string_view name);

    // Escape hatch: the underlying network, for statistics and now().
    // Creates the network (with the default seed 0) if it does not exist yet.
    simulnetx::network& network();

    // Run the network to virtual time t. Throws std::invalid_argument if t is
    // before the current virtual time.
    scenario& run_until(std::chrono::nanoseconds t);

private:
    simulnetx::network& ensure_network();
    void add_node_named(std::string_view name);
    simulnetx::node& resolve_node(std::string_view name);

    std::string name_;
    std::uint64_t seed_{0};
    std::unique_ptr<simulnetx::network> net_;
    // Transparent comparator so lookups take string_view without copying.
    std::map<std::string, simulnetx::node*, std::less<>> nodes_;
};

}  // namespace simulnetx
