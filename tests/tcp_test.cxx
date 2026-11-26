#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cstring>
#include <sstream>
#include <string>
#include <vector>

#include "simulnetx/network.hxx"
#include "simulnetx/serializer.hxx"
#include "simulnetx/tcp.hxx"
#include "simulnetx/udp.hxx"

using namespace simulnetx;
using namespace std::chrono_literals;

namespace {

// Two nodes with a plain 20ms link. The network is the first member so
// the node references (bound in the constructor body) stay valid.
struct TcpPair {
    network net;
    node& a;
    node& b;

    explicit TcpPair(std::uint64_t seed = 0)
        : net(seed), a(net.add_node("A")), b(net.add_node("B")) {
        net.link(a.id(), b.id(), link_config{.latency = 20ms});
    }
};


// A listening acceptor on node B, port 9000, plus a client socket on A.
struct TcpServer {
    TcpPair& pair;
    tcp::acceptor acceptor;

    explicit TcpServer(TcpPair& p) : pair(p), acceptor(p.net, p.b) {
        acceptor.bind(9000);
        acceptor.listen(8);
    }
};

// Connects a client socket on A to B:9000 and arms the server's accept.
// Returns the client socket. The accept handler stores the accepted
// socket in `accepted` (and its error in `accept_ec`).
tcp::socket connect_client(TcpPair& p, TcpServer& server, tcp::socket& accepted,
                           std::error_code& accept_ec, std::error_code& connect_ec) {
    tcp::socket client(p.net, p.a);
    server.acceptor.async_accept(
        [&accepted, &accept_ec](tcp::socket s, std::error_code ec) {
            accepted = std::move(s);
            accept_ec = ec;
        });
    client.async_connect(tcp::endpoint{p.b.id(), 9000},
                         [&connect_ec](std::error_code ec) { connect_ec = ec; });
    return client;
}

}  // namespace

TEST(TcpTest, BindAndLocalEndpoint) {
    TcpPair p;
    tcp::socket s(p.net, p.a);
    s.bind(9000);
    EXPECT_EQ(s.local_endpoint(), (tcp::endpoint{p.a.id(), 9000}));
}

TEST(TcpTest, DuplicateBindThrows) {
    TcpPair p;
    tcp::socket first(p.net, p.a);
    first.bind(9000);
    tcp::socket second(p.net, p.a);
    EXPECT_THROW(second.bind(9000), std::logic_error);
}

TEST(TcpTest, EphemeralBindAllocatesFrom49152) {
    TcpPair p;
    tcp::socket first(p.net, p.a);
    first.bind(0);
    EXPECT_EQ(first.local_endpoint(), (tcp::endpoint{p.a.id(), 49152}));
    tcp::socket second(p.net, p.a);
    second.bind(0);
    EXPECT_EQ(second.local_endpoint(), (tcp::endpoint{p.a.id(), 49153}));
}

TEST(TcpTest, LocalEndpointThrowsWhenUnbound) {
    TcpPair p;
    tcp::socket s(p.net, p.a);
    EXPECT_THROW(s.local_endpoint(), std::logic_error);
}

TEST(TcpTest, UdpAndTcpPortsAreIndependent) {
    // Separate registries per protocol: TCP 9000 and UDP 9000 coexist.
    TcpPair p;
    tcp::socket t(p.net, p.a);
    t.bind(9000);
    udp::socket u(p.net, p.a);
    EXPECT_NO_THROW(u.bind(9000));
    EXPECT_EQ(u.local_endpoint(), (udp::endpoint{p.a.id(), 9000}));
}

TEST(TcpTest, AcceptorBindListen) {
    TcpPair p;
    tcp::acceptor acc(p.net, p.b);
    acc.bind(9000);
    EXPECT_NO_THROW(acc.listen(8));
    EXPECT_EQ(acc.local_endpoint(), (tcp::endpoint{p.b.id(), 9000}));
}

TEST(TcpTest, EnvelopeTagIsStable) {
    // The TCP KIND tag is FNV-1a("simulnetx::tcp"), cross-compiler stable.
    EXPECT_EQ(detail::tcp_envelope_tag, 14926712757373340827ull);
    EXPECT_EQ(detail::datagram_envelope_tag, 5828304564438402943ull);
}

TEST(TcpTest, HandshakeGolden) {
    TcpPair p;
    TcpServer server(p);
    tcp::socket accepted(p.net, p.b);
    std::error_code accept_ec{1, std::generic_category()};
    std::error_code connect_ec{1, std::generic_category()};
    tcp::socket client = connect_client(p, server, accepted, accept_ec, connect_ec);

    // No handler runs before run_until.
    EXPECT_EQ(connect_ec.value(), 1);
    EXPECT_EQ(accept_ec.value(), 1);

    p.net.run_until(200ms);
    EXPECT_FALSE(connect_ec);
    EXPECT_FALSE(accept_ec);
    EXPECT_EQ(client.remote_endpoint(), (tcp::endpoint{p.b.id(), 9000}));
    EXPECT_EQ(accepted.remote_endpoint(), (tcp::endpoint{p.a.id(), 49152}));

    // 3 segments (SYN, SYN+ACK, ACK) -> 6 trace events, none dropped.
    EXPECT_EQ(static_cast<std::size_t>(p.net.event_count(event_kind::sent)), 3u);
    EXPECT_EQ(static_cast<std::size_t>(p.net.event_count(event_kind::delivered)), 3u);
    EXPECT_EQ(static_cast<std::size_t>(p.net.event_count(event_kind::dropped)), 0u);
    EXPECT_EQ(p.net.trace().size(), 6u);
}

TEST(TcpTest, Refused) {
    TcpPair p;
    tcp::socket client(p.net, p.a);
    std::error_code connect_ec{1, std::generic_category()};
    client.async_connect(tcp::endpoint{p.b.id(), 9000},
                         [&connect_ec](std::error_code ec) { connect_ec = ec; });
    p.net.run_until(200ms);
    EXPECT_EQ(connect_ec, std::make_error_code(std::errc::connection_refused));
    // The SYN was sent, the RST came back: 2 sent, 2 delivered.
    EXPECT_EQ(static_cast<std::size_t>(p.net.event_count(event_kind::sent)), 2u);
    EXPECT_EQ(static_cast<std::size_t>(p.net.event_count(event_kind::delivered)), 2u);
}

TEST(TcpTest, SelfConnect) {
    // A client and a listener on the SAME node: the SYN is a loopback
    // through the simulated link to itself.
    TcpPair p;
    tcp::acceptor acceptor(p.net, p.a);
    acceptor.bind(9000);
    acceptor.listen(8);
    tcp::socket accepted(p.net, p.a);
    std::error_code accept_ec{1, std::generic_category()};
    acceptor.async_accept([&accepted, &accept_ec](tcp::socket s, std::error_code ec) {
        accepted = std::move(s);
        accept_ec = ec;
    });
    tcp::socket client(p.net, p.a);
    std::error_code connect_ec{1, std::generic_category()};
    client.async_connect(tcp::endpoint{p.a.id(), 9000},
                         [&connect_ec](std::error_code ec) { connect_ec = ec; });
    p.net.run_until(200ms);
    EXPECT_FALSE(connect_ec);
    EXPECT_FALSE(accept_ec);
    EXPECT_EQ(client.remote_endpoint(), (tcp::endpoint{p.a.id(), 9000}));
}

TEST(TcpTest, TcpTagIsStable) {
    TcpPair p;
    TcpServer server(p);
    tcp::socket accepted(p.net, p.b);
    std::error_code accept_ec{1, std::generic_category()};
    std::error_code connect_ec{1, std::generic_category()};
    tcp::socket client = connect_client(p, server, accepted, accept_ec, connect_ec);
    p.net.run_until(200ms);
    ASSERT_FALSE(connect_ec);
    // Every TCP segment carries the FNV-1a tag of "simulnetx::tcp".
    for (const trace_event& ev : p.net.trace()) {
        if (ev.kind == event_kind::sent || ev.kind == event_kind::delivered) {
            EXPECT_EQ(ev.tag, 14926712757373340827ull);
        }
    }
}

std::string from_bytes(const std::vector<std::byte>& bytes) {
    std::string text;
    text.reserve(bytes.size());
    for (const std::byte b : bytes) {
        text.push_back(static_cast<char>(b));
    }
    return text;
}

TEST(TcpTest, StreamEcho) {
    TcpPair p;
    TcpServer server(p);
    tcp::socket accepted(p.net, p.b);
    std::error_code accept_ec{1, std::generic_category()};
    std::error_code connect_ec{1, std::generic_category()};
    tcp::socket client = connect_client(p, server, accepted, accept_ec, connect_ec);
    p.net.run_until(200ms);
    ASSERT_FALSE(connect_ec);
    ASSERT_FALSE(accept_ec);

    std::array<std::byte, 16> client_buf{};
    std::array<std::byte, 16> server_buf{};
    std::error_code client_read_ec{1, std::generic_category()};
    std::error_code server_read_ec{1, std::generic_category()};
    std::size_t client_read = 99;
    std::size_t server_read = 99;
    std::error_code server_write_ec{1, std::generic_category()};
    std::size_t server_written = 99;

    client.async_write("hello", [](std::error_code ec, std::size_t) {
        EXPECT_FALSE(ec);  // no-op check inside the handler
    });
    accepted.async_read_some(server_buf, [&](std::error_code ec, std::size_t n) {
        server_read_ec = ec;
        server_read = n;
        if (!ec && n > 0) {
            accepted.async_write(
                std::span<const std::byte>(server_buf.data(), n),
                [&](std::error_code wec, std::size_t wn) {
                    server_write_ec = wec;
                    server_written = wn;
                });
        }
    });
    client.async_read_some(client_buf, [&](std::error_code ec, std::size_t n) {
        client_read_ec = ec;
        client_read = n;
    });

    p.net.run_until(500ms);
    EXPECT_FALSE(server_read_ec);
    EXPECT_EQ(server_read, 5u);
    EXPECT_EQ(from_bytes({server_buf.begin(), server_buf.begin() + 5}), "hello");
    EXPECT_FALSE(server_write_ec);
    EXPECT_EQ(server_written, 5u);
    EXPECT_FALSE(client_read_ec);
    EXPECT_EQ(client_read, 5u);
    EXPECT_EQ(from_bytes({client_buf.begin(), client_buf.begin() + 5}), "hello");
}

TEST(TcpTest, NoMessageBoundaries) {
    TcpPair p;
    TcpServer server(p);
    tcp::socket accepted(p.net, p.b);
    std::error_code accept_ec{1, std::generic_category()};
    std::error_code connect_ec{1, std::generic_category()};
    tcp::socket client = connect_client(p, server, accepted, accept_ec, connect_ec);
    p.net.run_until(200ms);
    ASSERT_FALSE(connect_ec);

    std::array<std::byte, 16> buf{};
    std::vector<std::byte> received;
    std::error_code read_ec{1, std::generic_category()};
    // async_read_some returns whatever is available (here the first
    // segment's 5 bytes); the stream has no message boundaries, so a
    // loop that reads until 10 bytes sees "helloworld" as one stream.
    std::function<void(std::error_code, std::size_t)> read_more =
        [&](std::error_code ec, std::size_t n) {
            read_ec = ec;
            if (ec || n == 0) {
                return;
            }
            received.insert(received.end(), buf.begin(), buf.begin() + n);
            if (received.size() < 10) {
                accepted.async_read_some(buf, read_more);
            }
        };
    accepted.async_read_some(buf, read_more);
    client.async_write("hello", [](std::error_code, std::size_t) {});
    client.async_write("world", [](std::error_code, std::size_t) {});

    p.net.run_until(500ms);
    EXPECT_FALSE(read_ec);
    EXPECT_EQ(received.size(), 10u);
    EXPECT_EQ(from_bytes(received), "helloworld");
}

TEST(TcpTest, MssSplit) {
    TcpPair p;
    TcpServer server(p);
    tcp::socket accepted(p.net, p.b);
    std::error_code accept_ec{1, std::generic_category()};
    std::error_code connect_ec{1, std::generic_category()};
    tcp::socket client = connect_client(p, server, accepted, accept_ec, connect_ec);
    p.net.run_until(200ms);
    ASSERT_FALSE(connect_ec);

    const std::string payload(3000, 'x');
    std::error_code write_ec{1, std::generic_category()};
    std::size_t written = 99;
    client.async_write(payload, [&](std::error_code ec, std::size_t n) {
        write_ec = ec;
        written = n;
    });

    std::vector<std::byte> received;
    std::array<std::byte, 512> buf{};
    std::error_code read_ec{1, std::generic_category()};
    std::function<void(std::error_code, std::size_t)> read_more =
        [&](std::error_code ec, std::size_t n) {
            read_ec = ec;
            if (ec || n == 0) {
                return;
            }
            received.insert(received.end(), buf.begin(), buf.begin() + n);
            if (received.size() < 3000) {
                accepted.async_read_some(buf, read_more);
            }
        };
    accepted.async_read_some(buf, read_more);

    p.net.run_until(2s);
    EXPECT_FALSE(write_ec);
    EXPECT_EQ(written, 3000u);
    EXPECT_EQ(received.size(), 3000u);
    EXPECT_TRUE(std::all_of(received.begin(), received.end(),
                            [](std::byte b) { return b == std::byte{'x'}; }));
    // 3000 bytes at MSS 1024 = 3 segments.
    EXPECT_EQ(static_cast<std::size_t>(p.net.event_count(event_kind::sent)), 3u + 3u + 3u);
}

TEST(TcpTest, ZeroLengthWrite) {
    TcpPair p;
    TcpServer server(p);
    tcp::socket accepted(p.net, p.b);
    std::error_code accept_ec{1, std::generic_category()};
    std::error_code connect_ec{1, std::generic_category()};
    tcp::socket client = connect_client(p, server, accepted, accept_ec, connect_ec);
    p.net.run_until(200ms);
    ASSERT_FALSE(connect_ec);

    const std::size_t sent_before = static_cast<std::size_t>(p.net.sent_count());
    std::error_code write_ec{1, std::generic_category()};
    std::size_t written = 99;
    client.async_write(std::span<const std::byte>{},
                       [&](std::error_code ec, std::size_t n) {
                           write_ec = ec;
                           written = n;
                       });
    p.net.run_until(300ms);
    EXPECT_FALSE(write_ec);
    EXPECT_EQ(written, 0u);
    EXPECT_EQ(static_cast<std::size_t>(p.net.sent_count()), sent_before);  // nothing sent
}

TEST(TcpTest, QueuedWritesInOrder) {
    TcpPair p;
    TcpServer server(p);
    tcp::socket accepted(p.net, p.b);
    std::error_code accept_ec{1, std::generic_category()};
    std::error_code connect_ec{1, std::generic_category()};
    tcp::socket client = connect_client(p, server, accepted, accept_ec, connect_ec);
    p.net.run_until(200ms);
    ASSERT_FALSE(connect_ec);

    std::array<std::byte, 16> buf{};
    std::vector<std::byte> received;
    std::error_code read_ec{1, std::generic_category()};
    std::function<void(std::error_code, std::size_t)> read_more =
        [&](std::error_code ec, std::size_t n) {
            read_ec = ec;
            if (ec || n == 0) {
                return;
            }
            received.insert(received.end(), buf.begin(), buf.begin() + n);
            if (received.size() < 3) {
                accepted.async_read_some(buf, read_more);
            }
        };
    accepted.async_read_some(buf, read_more);
    client.async_write("a", [](std::error_code, std::size_t) {});
    client.async_write("b", [](std::error_code, std::size_t) {});
    client.async_write("c", [](std::error_code, std::size_t) {});

    p.net.run_until(500ms);
    EXPECT_FALSE(read_ec);
    EXPECT_EQ(received.size(), 3u);
    EXPECT_EQ(from_bytes(received), "abc");
}

TEST(TcpTest, PartialRead) {
    TcpPair p;
    TcpServer server(p);
    tcp::socket accepted(p.net, p.b);
    std::error_code accept_ec{1, std::generic_category()};
    std::error_code connect_ec{1, std::generic_category()};
    tcp::socket client = connect_client(p, server, accepted, accept_ec, connect_ec);
    p.net.run_until(200ms);
    ASSERT_FALSE(connect_ec);

    client.async_write("0123456789", [](std::error_code, std::size_t) {});
    std::array<std::byte, 4> small{};
    std::array<std::byte, 16> rest{};
    std::size_t first_n = 99;
    std::size_t second_n = 99;
    accepted.async_read_some(small, [&](std::error_code ec, std::size_t n) {
        EXPECT_FALSE(ec);
        first_n = n;
        accepted.async_read_some(rest, [&](std::error_code ec2, std::size_t n2) {
            EXPECT_FALSE(ec2);
            second_n = n2;
        });
    });

    p.net.run_until(500ms);
    EXPECT_EQ(first_n, 4u);
    EXPECT_EQ(from_bytes({small.begin(), small.end()}), "0123");
    EXPECT_EQ(second_n, 6u);
    EXPECT_EQ(from_bytes({rest.begin(), rest.begin() + 6}), "456789");
}

TEST(TcpTest, MoveWithPendingRead) {
    TcpPair p;
    TcpServer server(p);
    tcp::socket accepted(p.net, p.b);
    std::error_code accept_ec{1, std::generic_category()};
    std::error_code connect_ec{1, std::generic_category()};
    tcp::socket client = connect_client(p, server, accepted, accept_ec, connect_ec);
    p.net.run_until(200ms);
    ASSERT_FALSE(connect_ec);

    std::array<std::byte, 16> buf{};
    std::error_code read_ec{1, std::generic_category()};
    std::size_t read_n = 99;
    accepted.async_read_some(buf, [&](std::error_code ec, std::size_t n) {
        read_ec = ec;
        read_n = n;
    });
    tcp::socket moved = std::move(accepted);  // move with a pending read
    client.async_write("hello", [](std::error_code, std::size_t) {});
    p.net.run_until(500ms);
    EXPECT_FALSE(read_ec);
    EXPECT_EQ(read_n, 5u);
    EXPECT_EQ(from_bytes({buf.begin(), buf.begin() + 5}), "hello");
}
