// UDP echo over the socket shim.
//
// A client sends "ping" to a server socket on port 8080; the server
// replies "pong" to the sender's address. 20ms latency, no loss, so the
// reply arrives at t=40ms and the trace is exactly 4 events: the ping
// sent and delivered, the pong sent and delivered. The client's socket
// is bound implicitly by its first send (an ephemeral port), so it
// sends before arming its receive - a receive on an unbound socket
// throws.
//
// Build: cmake -S . -B build && cmake --build build --target udp_echo
// Run:   ./build/udp_echo

#include <array>
#include <chrono>
#include <cstddef>
#include <cstring>
#include <iostream>
#include <string>
#include <string_view>
#include <vector>

#include "simulnetx/network.hxx"
#include "simulnetx/udp.hxx"

namespace {

using namespace std::chrono_literals;

using simulnetx::udp::endpoint;
using simulnetx::udp::socket;

std::vector<std::byte> as_bytes(std::string_view text) {
    std::vector<std::byte> out;
    out.reserve(text.size());
    for (const char c : text) {
        out.push_back(static_cast<std::byte>(c));
    }
    return out;
}

int run() {
    simulnetx::network net(42);
    auto& client_node = net.add_node("client");
    auto& server_node = net.add_node("server");
    net.link(client_node.id(), server_node.id(), {.latency = 20ms});

    socket server_sock(net, server_node);
    server_sock.bind(8080);
    std::array<std::byte, 64> server_buf{};
    server_sock.async_receive_from(
        server_buf, [&](std::error_code ec, std::size_t n, endpoint from) {
            if (ec) {
                return;
            }
            std::cout << "server got " << n << " bytes from "
                      << simulnetx::udp::to_string(from) << '\n';
            server_sock.async_send_to(as_bytes("pong"), from, [](auto, auto) {});
        });

    socket client_sock(net, client_node);
    std::array<std::byte, 64> client_buf{};
    std::string pong;
    long long pong_at = -1;
    // Send first, then arm the receive: the socket is unbound until its
    // first send, and a receive on an unbound socket throws.
    client_sock.async_send_to(as_bytes("ping"), {server_node.id(), 8080},
                              [](auto, auto) {});
    client_sock.async_receive_from(
        client_buf, [&](std::error_code ec, std::size_t n, endpoint /*from*/) {
            if (ec) {
                return;
            }
            pong.resize(n);
            if (n > 0) {
                std::memcpy(pong.data(), client_buf.data(), n);
            }
            pong_at = net.now().count();
        });

    net.run_until(1s);

    std::cout << "client got '" << pong << "' at t=" << pong_at << "ns\n";
    std::cout << "sent=" << static_cast<std::size_t>(net.sent_count())
              << " delivered=" << static_cast<std::size_t>(net.delivered_count())
              << " dropped=" << static_cast<std::size_t>(net.dropped_count()) << '\n';
    std::cout << net.dump_trace();

    simulnetx::expect(pong == "pong").equal(true);
    simulnetx::expect(pong_at).equal(40000000LL);
    net.sent_count().equal(2);
    net.delivered_count().equal(2);
    net.dropped_count().equal(0);
    simulnetx::expect(net.trace().size()).equal(4u);
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
