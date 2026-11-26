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
