# Scenario builder

The scenario builder is a fluent facade over `network`: a whole test reads as
one declaration. Each step executes immediately, in chain order; scheduling
before running is the caller's job.

```cpp
#include "simulnetx/scenario.hxx"

using namespace std::chrono_literals;

int main() {
    simulnetx::scenario("retry under 30% loss")
        .seed(42)
        .nodes("A", "B")
        .link("A", "B", {.latency = 50ms, .loss_rate = 0.30})
        .send_at(0s, "A", "B", "ping")
        .partition_at(5s, {"A"}, {"B"})
        .heal_at(8s)
        .run_until(10s)
        .expect_delivered(1);
}
```

The scheduled ping is the message the final check counts. A failed check
throws `simulnetx::assertion_failed` with the scenario name, the seed and the
virtual time in the message:

```
scenario 'retry under 30% loss': expected delivered_count == 1, got 0 (seed 42, t=10000000000ns)
```

`node("A")` and `network()` are escape hatches for full control (handlers,
timers, direct sends, statistics). The builder is a facade, not a test
framework: use it inside whatever test runner you like.

## Topology

Shorthand for common link layouts, operating on the nodes added so far, in
insertion order:

```cpp
simulnetx::scenario("cluster")
    .seed(1)
    .nodes("A", "B", "C", "D")
    .mesh({.latency = 10ms});       // every pair linked
    // .star("A", {...});           // A linked to every other node
    // .chain({...});               // A <-> B <-> C <-> D
```

`mesh` and `chain` need at least 2 nodes, `star` needs a hub plus at least
1 other node; fewer throws `std::logic_error`. `mesh` also takes an
explicit list of names to link a subset.
