// tcp_echo: a TCP-like reliable stream over the simulated network.
// A client connects to a server, sends "ping", the server echoes
// "pong" back. The stream delivers bytes in order with no message
// boundaries, even under loss, duplication, and reordering.
#include <array>
#include <chrono>
#include <iostream>
#include <string>

#include "simulnetx/network.hxx"
#include "simulnetx/tcp.hxx"

using namespace simulnetx;
using namespace std::chrono_literals;

namespace {

std::string from_bytes(const std::byte* begin, std::size_t n) {
    std::string text;
    text.reserve(n);
    for (std::size_t i = 0; i < n; ++i) {
        text.push_back(static_cast<char>(begin[i]));
    }
    return text;
}

}  // namespace

int main() {
    network net(0);
    node& client_node = net.add_node("client");
    node& server_node = net.add_node("server");
    net.link(client_node.id(), server_node.id(), link_config{.latency = 20ms});

    tcp::acceptor acceptor(net, server_node);
    acceptor.bind(9000);
    acceptor.listen(8);

    tcp::socket server_side(net, server_node);
    acceptor.async_accept([&server_side](tcp::socket s, std::error_code ec) {
        if (ec) {
            std::cout << "accept failed: " << ec.message() << '\n';
            return;
        }
        server_side = std::move(s);
        std::cout << "server accepted " << to_string(server_side.remote_endpoint())
                  << '\n';
        auto buf = std::make_shared<std::array<std::byte, 16>>();
        server_side.async_read_some(
            *buf, [&server_side, buf](std::error_code ec, std::size_t n) {
                if (ec) {
                    std::cout << "server read failed: " << ec.message() << '\n';
                    return;
                }
                std::cout << "server got '" << from_bytes(buf->data(), n) << "'\n";
                server_side.async_write(
                    std::span<const std::byte>(buf->data(), n),
                    [](std::error_code ec, std::size_t written) {
                        if (ec) {
                            std::cout << "server write failed: " << ec.message()
                                      << '\n';
                            return;
                        }
                        std::cout << "server echoed " << written << " bytes\n";
                    });
            });
    });

    tcp::socket client(net, client_node);
    client.async_connect(tcp::endpoint{server_node.id(), 9000},
                         [&client, &net](std::error_code ec) {
                             if (ec) {
                                 std::cout << "connect failed: " << ec.message()
                                           << '\n';
                                 return;
                             }
                             std::cout << "client connected to "
                                       << to_string(client.remote_endpoint()) << '\n';
                             client.async_write(
                                 "ping", [](std::error_code ec, std::size_t n) {
                                     if (ec) {
                                         std::cout << "client write failed: "
                                                   << ec.message() << '\n';
                                         return;
                                     }
                                     std::cout << "client wrote " << n << " bytes\n";
                                 });
                             auto buf = std::make_shared<std::array<std::byte, 16>>();
                             client.async_read_some(
                                 *buf, [&net, buf](std::error_code ec, std::size_t n) {
                                     if (ec) {
                                         std::cout << "client read failed: "
                                                   << ec.message() << '\n';
                                         return;
                                     }
                                     std::cout << "client got '"
                                               << from_bytes(buf->data(), n) << "' at t="
                                               << net.now().count() << "ns\n";
                                 });
                         });

    net.run_until(1s);
    std::cout << "sent=" << static_cast<std::size_t>(net.sent_count())
              << " delivered=" << static_cast<std::size_t>(net.delivered_count())
              << " dropped=" << static_cast<std::size_t>(net.dropped_count()) << '\n';
    std::cout << net.dump_trace();
    return 0;
}
