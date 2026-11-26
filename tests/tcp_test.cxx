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
