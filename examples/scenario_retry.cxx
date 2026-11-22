// Retry protocol under loss and a partition, written with the scenario API.
//
// Same protocol as examples/retry_under_loss.cxx: A sends "ping" to B and
// retransmits every 100ms until B's "ack" arrives. The link drops 30% of
// messages in both directions, and the network is partitioned from t=200ms
// to t=1s. With a fixed seed the whole run is reproducible: same seed, same
// attempt count, same ack time.
//
// The scenario builder covers setup, fault scheduling and checks; the retry
// loop itself uses the node() escape hatch, because it is stateful logic
// rather than a declarative step.
//
// Build: cmake -S . -B build && cmake --build build --target scenario_retry
// Run:   ./build/scenario_retry

#include <chrono>
#include <functional>
#include <iostream>

#include "simulnetx/scenario.hxx"

namespace {

using namespace std::chrono_literals;

int run() {
    simulnetx::scenario s("retry under 30% loss");
    s.seed(42)
        .nodes("A", "B")
        .link("A", "B", {.latency = 20ms, .loss_rate = 0.3})
        .partition_at(200ms, {"A"}, {"B"})
        .heal_at(1s);

    auto& a = s.node("A");
    auto& b = s.node("B");

    bool acked = false;
    long long acked_at = -1;
    int attempts = 0;

    s.on_message("B", [&](const simulnetx::message& m) {
        if (m.payload.size() == 4) {  // "ping"
            b.send(a.id(), "ack");
        }
    });
    s.on_message("A", [&](const simulnetx::message& m) {
        if (m.payload.size() == 3) {  // "ack"
            acked = true;
            acked_at = s.network().now().count();
        }
    });

    std::function<void()> retry;
    retry = [&] {
        if (acked) {
            return;
        }
        ++attempts;
        a.send(b.id(), "ping");
        a.after(100ms, retry);
    };
    a.after(0ns, retry);

    s.run_until(5s);

    std::cout << "acked: " << (acked ? "yes" : "no") << " after " << attempts
              << " attempts, ack arrived at t=" << acked_at << "ns\n";
    std::cout << "sent=" << static_cast<std::size_t>(s.network().sent_count())
              << " delivered=" << static_cast<std::size_t>(s.network().delivered_count())
              << " dropped=" << static_cast<std::size_t>(s.network().dropped_count()) << '\n';

    // Golden values for seed 42 (same run as retry_under_loss.cxx).
    simulnetx::expect(acked).equal(true);
    s.expect_sent(17).expect_delivered(5).expect_dropped(12);
    return 0;
}

}  // namespace

int main() {
    try {
        return run();
    } catch (const simulnetx::assertion_failed& e) {
        std::cerr << "assertion failed: " << e.what() << '\n';
        return 1;
    }
}
