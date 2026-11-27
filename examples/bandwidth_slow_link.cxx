// bandwidth_slow_link: three 1000-byte messages over a 1000 Bps link.
// Each needs 8s on the wire (1000 * 8 * 1e9 / 1000 ns) and queues
// behind the previous one, so the arrivals spread to t=8s, t=16s, t=24s.
#include <chrono>
#include <cstddef>
#include <iostream>
#include <vector>

#include "simulnetx/network.hxx"

using namespace simulnetx;
using namespace std::chrono_literals;

int main() {
    network net(0);
    node& a = net.add_node("A");
    node& b = net.add_node("B");
    net.link(a.id(), b.id(), {.bandwidth_bps = 1000});

    const std::vector<std::byte> payload(1000, std::byte{'x'});
    a.send(b.id(), payload);
    a.send(b.id(), payload);
    a.send(b.id(), payload);
    net.run_until(30s);
    std::cout << "sent=" << static_cast<std::size_t>(net.sent_count())
              << " delivered=" << static_cast<std::size_t>(net.delivered_count())
              << " dropped=" << static_cast<std::size_t>(net.dropped_count()) << '\n';
    std::cout << net.dump_trace();
    return 0;
}
