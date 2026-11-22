// Retry protocol under loss and a partition.
//
// A sends "ping" to B and retransmits every 100ms until B's "ack" arrives.
// The link drops 30% of messages in both directions, and the network is
// partitioned from t=200ms to t=1s. With a fixed seed the whole run is
// reproducible: same seed, same attempt count, same ack time.
//
// Build: cmake -S . -B build && cmake --build build --target retry_under_loss
// Run:   ./build/retry_under_loss

#include <chrono>
#include <functional>
#include <iostream>

#include "simulnetx/network.hxx"

namespace {

using namespace std::chrono_literals;

int run() {
    simulnetx::network net(42);
    auto& a = net.add_node("A");
    auto& b = net.add_node("B");
    net.link(a.id(), b.id(), {.latency = 20ms, .loss_rate = 0.3});

    bool acked = false;
    long long acked_at = -1;
    int attempts = 0;

    b.on_message([&](const simulnetx::message& m) {
        if (m.payload.size() == 4) {  // "ping"
            b.send(a.id(), "ack");
        }
    });
    a.on_message([&](const simulnetx::message& m) {
        if (m.payload.size() == 3) {  // "ack"
            acked = true;
            acked_at = net.now().count();
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

    // Cut the network for 800ms in the middle of the run.
    a.after(200ms, [&] { net.partition({a.id()}, {b.id()}); });
    a.after(1s, [&] { net.heal(); });

    net.run_for(5s);

    std::cout << "acked: " << (acked ? "yes" : "no") << " after " << attempts
              << " attempts, ack arrived at t=" << acked_at << "ns\n";
    std::cout << "sent=" << static_cast<std::size_t>(net.sent_count())
              << " delivered=" << static_cast<std::size_t>(net.delivered_count())
              << " dropped=" << static_cast<std::size_t>(net.dropped_count()) << '\n';

    // The protocol must converge, and the run must have seen real loss.
    simulnetx::expect(acked).equal(true);
    net.delivered_count().greater_than(0);
    net.dropped_count().greater_than(0);
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
