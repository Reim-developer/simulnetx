// Primary/backup heartbeat failover, written with the scenario API.
//
// A (primary) sends a heartbeat to B (backup) every 1s over a DIRECTED link
// A -> B. A crashes at t=5s; B notices the missed heartbeats and takes over
// at t=7s. A restarts at t=8s and rejoins, and B steps down.
//
// Two v0.3.0 behaviors are on show:
// - A's heartbeat timers die with the crash (node timers belong to the dead
//   incarnation), so A re-arms its heartbeat loop after the restart. The
//   re-arm uses a NETWORK timer, which survives the crash.
// - B's checker is a node timer on B, which never crashes, so it keeps
//   running through the whole outage.
//
// Build: cmake -S . -B build && cmake --build build --target failover
// Run:   ./build/failover

#include <chrono>
#include <functional>
#include <iostream>

#include "simulnetx/scenario.hxx"

namespace {

using namespace std::chrono_literals;

int run() {
    simulnetx::scenario s("heartbeat failover");
    s.seed(7)
        .nodes("A", "B")
        .link_directed("A", "B", {.latency = 10ms})
        .crash_at(5s, "A")
        .restart_at(8s, "A");

    auto& a = s.node("A");
    auto& b = s.node("B");

    long long last_beat = -1;  // arrival time of the last heartbeat, -1 = none
    bool taken_over = false;

    s.on_message("B", [&](const simulnetx::message&) {
        const long long now = s.network().now().count();
        if (taken_over) {
            std::cout << "B: A back at t=" << now << "ns, stepping down\n";
            taken_over = false;
        }
        last_beat = now;
    });

    // A sends a heartbeat every 1s until it crashes. The loop is a node
    // timer: it dies with the crash and must be re-armed after the restart.
    std::function<void()> beat;
    beat = [&] {
        a.send(b.id(), "beat");
        a.after(1s, beat);
    };
    a.after(0ns, beat);

    // B checks for missed heartbeats every 1s. A gap over 2s means A is down.
    std::function<void()> check;
    check = [&] {
        const long long now = s.network().now().count();
        if (!taken_over && last_beat >= 0 && now - last_beat > 2000000000LL) {
            std::cout << "B: takeover at t=" << now << "ns (last heartbeat "
                      << (now - last_beat) << "ns ago)\n";
            taken_over = true;
        }
        b.after(1s, check);
    };
    b.after(1s, check);

    // Re-arm A's heartbeat loop after the restart. A network timer: it
    // survives A's crash, and it fires after the restart because it was
    // scheduled after restart_at (same virtual time, later sequence).
    s.network().schedule_at(8s, [&] { a.after(0ns, beat); });

    s.run_until(10s);

    std::cout << "sent=" << static_cast<std::size_t>(s.network().sent_count())
              << " delivered=" << static_cast<std::size_t>(s.network().delivered_count())
              << " dropped=" << static_cast<std::size_t>(s.network().dropped_count()) << '\n';

    // Golden values for seed 7: 5 beats before the crash, 3 after the
    // restart (the 10s beat is sent but its 10.01s arrival is past the run).
    s.expect_sent(8).expect_delivered(7).expect_dropped(0);
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
