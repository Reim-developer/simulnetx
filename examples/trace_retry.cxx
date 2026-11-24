// Event tracing on the retry protocol.
//
// The same run as retry_under_loss (A pings B every 100ms until the ack
// arrives; 30% loss both ways; partitioned 200ms-1s), with the event
// trace printed afterwards: every send, delivery, drop (with its reason),
// timer firing, partition and heal, in virtual-time order. The counts
// say the run failed or passed; the trace says why.
//
// Build: cmake -S . -B build && cmake --build build --target trace_retry
// Run:   ./build/trace_retry

#include <algorithm>
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

    // The trace answers what the counts cannot: which drops were the
    // partition, which were loss.
    const auto& trace = net.trace();
    const auto partition_drops =
        std::count_if(trace.begin(), trace.end(), [](const simulnetx::trace_event& e) {
            return e.kind == simulnetx::event_kind::dropped &&
                   e.reason == simulnetx::drop_reason::partition;
        });
    const auto loss_drops =
        std::count_if(trace.begin(), trace.end(), [](const simulnetx::trace_event& e) {
            return e.kind == simulnetx::event_kind::dropped &&
                   e.reason == simulnetx::drop_reason::loss;
        });
    std::cout << "dropped by partition: " << partition_drops
              << ", dropped by loss: " << loss_drops << '\n';
    std::cout << net.dump_trace();

    // The protocol must converge, and the run must have seen real loss.
    simulnetx::expect(acked).equal(true);
    net.delivered_count().greater_than(0);
    net.dropped_count().greater_than(0);
    net.event_count(simulnetx::event_kind::sent).equal(17);
    net.event_count(simulnetx::event_kind::timer_fired).equal(16);
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
