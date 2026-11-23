// Typed ping/pong under loss, written with the request/response engine.
//
// The client sends a typed hello request to the server, which replies with
// a typed world response. The link drops 30% of messages in both
// directions; the request retries every 1s until the response arrives or
// the 10s timeout fires. With a fixed seed the whole run is reproducible:
// same seed, same answer time, same counts.
//
// Compare examples/scenario_retry.cxx: the same protocol written by hand
// needs an explicit retry loop, an ack check, and a state flag. The
// request engine owns all of that; the test only declares what it wants.
//
// Build: cmake -S . -B build && cmake --build build --target typed_ping_pong
// Run:   ./build/typed_ping_pong

#include <chrono>
#include <iostream>
#include <string>

#include "simulnetx/scenario.hxx"

namespace {

using namespace std::chrono_literals;

struct hello {
    std::string name;
};

struct world {
    std::string text;
};

int run() {
    int served = 0;
    simulnetx::scenario sc("typed ping/pong");
    sc.seed(42)
        .nodes("client", "server")
        .link("client", "server", {.latency = 50ms, .loss_rate = 0.30})
        .serve<hello, world>("server", [&served](const hello& h) {
            ++served;
            return world{"hello, " + h.name};
        });

    bool answered = false;
    std::string answer;
    long long answered_at = -1;

    sc.request<hello, world>("client", "server", hello{"reim"})
        .retry_every(1s)
        .timeout_after(10s)
        .on_response([&](const world& w) {
            answered = true;
            answer = w.text;
            answered_at = sc.network().now().count();
        })
        .on_timeout([&] { answered = false; });

    sc.run_until(10s);

    std::cout << "answer: " << (answered ? answer : "(timed out)")
              << ", arrived at t=" << answered_at << "ns, server served " << served
              << " request(s)\n";
    std::cout << "sent=" << static_cast<std::size_t>(sc.network().sent_count())
              << " delivered=" << static_cast<std::size_t>(sc.network().delivered_count())
              << " dropped=" << static_cast<std::size_t>(sc.network().dropped_count()) << '\n';

    // Golden values for seed 42 (read from the first run, pinned here):
    // the request sent at t=4s is the first whose reply survives, so the
    // answer arrives at 4s + 50ms + 50ms. 5 requests went out, 4 arrived
    // (1 lost); 4 replies went out, 1 arrived (3 lost).
    simulnetx::expect(answered).equal(true);
    simulnetx::expect(answered_at).equal(4100000000LL);
    simulnetx::expect(served).equal(4);
    sc.expect_sent(9).expect_delivered(5).expect_dropped(4);
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
