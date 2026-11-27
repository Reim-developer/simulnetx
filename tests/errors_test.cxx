// ErrorsTest: every row of docs/errors.md that was unpinned by another
// test file gets pinned here. Tests are grouped by the exception type
// they pin; each test names the throw site it covers in its comment.
#include <array>
#include <chrono>
#include <cstddef>
#include <limits>
#include <stdexcept>
#include <string>
#include <string_view>
#include <system_error>
#include <type_traits>
#include <vector>

#include <gtest/gtest.h>

#include "simulnetx/errors.hxx"
#include "simulnetx/message.hxx"
#include "simulnetx/network.hxx"
#include "simulnetx/tcp.hxx"
#include "simulnetx/udp.hxx"

namespace {

using namespace std::chrono_literals;
using namespace simulnetx;

struct hello {
    std::string text;
};
struct world {
    std::string text;
};

struct TwoNodes {
    network net;
    node& a;
    node& b;

    explicit TwoNodes(std::uint64_t seed = 0)
        : net(seed), a(net.add_node("A")), b(net.add_node("B")) {
        net.link(a.id(), b.id(), {.latency = 20ms});
    }
};

struct TcpPair {
    network net;
    node& a;
    node& b;

    explicit TcpPair(std::uint64_t seed = 0)
        : net(seed), a(net.add_node("A")), b(net.add_node("B")) {
        net.link(a.id(), b.id(), {.latency = 20ms});
    }
};

tcp::socket connect_client(TcpPair& p, tcp::acceptor& acceptor, tcp::socket& accepted,
                           std::error_code& accept_ec, std::error_code& connect_ec) {
    acceptor.async_accept([&accepted, &accept_ec](tcp::socket s, std::error_code ec) {
        accepted = std::move(s);
        accept_ec = ec;
    });
    tcp::socket client(p.net, p.a);
    client.async_connect(tcp::endpoint{p.b.id(), 9000},
                         [&connect_ec](std::error_code ec) { connect_ec = ec; });
    return client;
}

}  // namespace

// --- std::logic_error: reentrant run_until (network.cxx) ---

TEST(ErrorsTest, ReentrantRunUntilThrowsLogicError) {
    TwoNodes t;
    bool run_until_threw = false;
    bool run_for_threw = false;
    t.a.after(10ms, [&] {
        try {
            t.net.run_until(20ms);
        } catch (const std::logic_error&) {
            run_until_threw = true;
        }
        try {
            t.net.run_for(10ms);
        } catch (const std::logic_error&) {
            run_for_threw = true;
        }
    });
    t.net.run_until(100ms);
    EXPECT_TRUE(run_until_threw);
    EXPECT_TRUE(run_for_threw);
    // The guard reset: the network is still usable afterwards.
    EXPECT_NO_THROW(t.net.run_until(200ms));
    EXPECT_EQ(t.net.now().count(), 200'000'000LL);
}

TEST(ErrorsTest, ReentrantThrowEscapingHandlerResetsGuard) {
    TwoNodes t;
    t.a.after(10ms, [&] { t.net.run_until(20ms); });
    EXPECT_THROW(t.net.run_until(100ms), std::logic_error);
    // The guard reset even though the throw escaped the outer run_until.
    EXPECT_NO_THROW(t.net.run_until(200ms));
    EXPECT_EQ(t.net.now().count(), 200'000'000LL);
}

// --- std::invalid_argument: virtual-time overflow (network.cxx checked_add) ---

TEST(ErrorsTest, SendNearMaxTimeThrowsInvalidArgument) {
    network net(0);
    node& a = net.add_node("A");
    node& b = net.add_node("B");
    net.link(a.id(), b.id(), {.latency = 2s});
    const auto near_max = std::chrono::nanoseconds{std::numeric_limits<std::int64_t>::max() -
                                                   1'000'000'000LL};
    net.run_until(near_max);
    EXPECT_THROW(a.send(b.id(), "x"), std::invalid_argument);
}

// --- decode_error : std::invalid_argument (errors.hxx) ---

static_assert(std::is_base_of_v<std::invalid_argument, decode_error>);
static_assert(!std::is_base_of_v<std::runtime_error, decode_error>);

TEST(ErrorsTest, DecodeErrorIsInvalidArgument) {
    TwoNodes t;
    t.b.on_message([&](const message& m) {
        EXPECT_THROW(m.as<world>(), decode_error);
        EXPECT_THROW(m.as<world>(), std::invalid_argument);
    });
    t.a.send<hello>(t.b.id(), hello{"hi"});
    t.net.run_until(1s);
}

// --- std::logic_error: UDP socket state violations (udp.cxx) ---

TEST(ErrorsTest, UdpOpsOnClosedSocketThrow) {
    TwoNodes t;
    udp::socket s(t.net, t.a);
    s.bind(8080);
    s.close();
    EXPECT_THROW(s.bind(8081), std::logic_error);
    EXPECT_THROW(s.connect(udp::endpoint{t.b.id(), 8000}), std::logic_error);
    std::array<std::byte, 4> buf{};
    EXPECT_THROW(s.async_send_to(std::span<const std::byte>(buf), udp::endpoint{t.b.id(), 8000},
                                 [](std::error_code, std::size_t) {}),
                 std::logic_error);
    EXPECT_THROW(s.async_receive_from(std::span<std::byte>(buf),
                                      [](std::error_code, std::size_t, udp::endpoint) {}),
                 std::logic_error);
}

TEST(ErrorsTest, UdpReceiveUnconnectedThrows) {
    TwoNodes t;
    udp::socket s(t.net, t.a);
    s.bind(8080);
    std::array<std::byte, 4> buf{};
    EXPECT_THROW(s.async_receive(std::span<std::byte>(buf),
                                 [](std::error_code, std::size_t) {}),
                 std::logic_error);
}

// --- std::logic_error: TCP socket state violations (tcp.cxx, tcp.hxx) ---

TEST(ErrorsTest, TcpBindOnClosedSocketThrows) {
    TwoNodes t;
    tcp::socket s(t.net, t.a);
    s.bind(9000);
    s.close();
    EXPECT_THROW(s.bind(9001), std::logic_error);
}

TEST(ErrorsTest, TcpDoubleBindThrows) {
    TwoNodes t;
    tcp::socket s(t.net, t.a);
    s.bind(9000);
    EXPECT_THROW(s.bind(9001), std::logic_error);
}

TEST(ErrorsTest, TcpLocalEndpointUnboundThrows) {
    TwoNodes t;
    tcp::socket s(t.net, t.a);
    EXPECT_THROW(s.local_endpoint(), std::logic_error);
}

TEST(ErrorsTest, TcpRemoteEndpointUnconnectedThrows) {
    TwoNodes t;
    tcp::socket s(t.net, t.a);
    s.bind(9000);
    EXPECT_THROW(s.remote_endpoint(), std::logic_error);
}

TEST(ErrorsTest, TcpConnectOnClosedSocketThrows) {
    TwoNodes t;
    tcp::socket s(t.net, t.a);
    s.close();
    EXPECT_THROW(s.async_connect(tcp::endpoint{t.b.id(), 9000},
                                 [](std::error_code) {}),
                 std::logic_error);
}

TEST(ErrorsTest, TcpConnectOnConnectedSocketThrows) {
    TcpPair p;
    tcp::acceptor acceptor(p.net, p.b);
    acceptor.bind(9000);
    acceptor.listen(8);
    tcp::socket accepted(p.net, p.b);
    std::error_code accept_ec{1, std::generic_category()};
    std::error_code connect_ec{1, std::generic_category()};
    tcp::socket client = connect_client(p, acceptor, accepted, accept_ec, connect_ec);
    p.net.run_until(200ms);
    ASSERT_FALSE(connect_ec);
    EXPECT_THROW(client.async_connect(tcp::endpoint{p.b.id(), 9000},
                                      [](std::error_code) {}),
                 std::logic_error);
}

TEST(ErrorsTest, TcpWriteOnUnconnectedSocketThrows) {
    TwoNodes t;
    tcp::socket s(t.net, t.a);
    EXPECT_THROW(s.async_write("x", [](std::error_code, std::size_t) {}), std::logic_error);
}

TEST(ErrorsTest, TcpReadOnUnconnectedSocketThrows) {
    TwoNodes t;
    tcp::socket s(t.net, t.a);
    std::array<std::byte, 16> buf{};
    EXPECT_THROW(s.async_read_some(std::span<std::byte>(buf),
                                   [](std::error_code, std::size_t) {}),
                 std::logic_error);
}

TEST(ErrorsTest, TcpSecondPendingReadThrows) {
    TcpPair p;
    tcp::acceptor acceptor(p.net, p.b);
    acceptor.bind(9000);
    acceptor.listen(8);
    tcp::socket accepted(p.net, p.b);
    std::error_code accept_ec{1, std::generic_category()};
    std::error_code connect_ec{1, std::generic_category()};
    tcp::socket client = connect_client(p, acceptor, accepted, accept_ec, connect_ec);
    p.net.run_until(200ms);
    ASSERT_FALSE(connect_ec);
    ASSERT_FALSE(accept_ec);
    std::array<std::byte, 16> buf{};
    std::array<std::byte, 16> buf2{};
    accepted.async_read_some(std::span<std::byte>(buf),
                             [](std::error_code, std::size_t) {});
    EXPECT_THROW(accepted.async_read_some(std::span<std::byte>(buf2),
                                          [](std::error_code, std::size_t) {}),
                 std::logic_error);
}

TEST(ErrorsTest, TcpMovedFromSocketThrows) {
    TwoNodes t;
    tcp::socket s(t.net, t.a);
    tcp::socket moved = std::move(s);
    EXPECT_THROW(s.local_endpoint(), std::logic_error);
    EXPECT_THROW(s.bind(9000), std::logic_error);
    EXPECT_THROW(s.async_connect(tcp::endpoint{t.b.id(), 9000},
                                 [](std::error_code) {}),
                 std::logic_error);
}

// --- std::logic_error: TCP acceptor state violations (tcp.cxx) ---

TEST(ErrorsTest, AcceptorBindOnClosedThrows) {
    TwoNodes t;
    tcp::acceptor acc(t.net, t.b);
    acc.bind(9000);
    acc.close();
    EXPECT_THROW(acc.bind(9001), std::logic_error);
}

TEST(ErrorsTest, AcceptorDoubleBindThrows) {
    TwoNodes t;
    tcp::acceptor acc(t.net, t.b);
    acc.bind(9000);
    EXPECT_THROW(acc.bind(9001), std::logic_error);
}

TEST(ErrorsTest, AcceptorListenErrorsThrow) {
    TwoNodes t;
    tcp::acceptor closed(t.net, t.b);
    closed.bind(9000);
    closed.close();
    EXPECT_THROW(closed.listen(8), std::logic_error);
    tcp::acceptor unbound(t.net, t.b);
    EXPECT_THROW(unbound.listen(8), std::logic_error);
    tcp::acceptor twice(t.net, t.b);
    twice.bind(9000);
    twice.listen(8);
    EXPECT_THROW(twice.listen(8), std::logic_error);
}

TEST(ErrorsTest, AcceptorLocalEndpointUnboundThrows) {
    TwoNodes t;
    tcp::acceptor acc(t.net, t.b);
    EXPECT_THROW(acc.local_endpoint(), std::logic_error);
}

TEST(ErrorsTest, AcceptorAcceptErrorsThrow) {
    TwoNodes t;
    tcp::acceptor closed(t.net, t.b);
    closed.bind(9000);
    closed.listen(8);
    closed.close();
    EXPECT_THROW(closed.async_accept([](tcp::socket, std::error_code) {}), std::logic_error);
    tcp::acceptor not_listening(t.net, t.b);
    not_listening.bind(9001);
    EXPECT_THROW(not_listening.async_accept([](tcp::socket, std::error_code) {}),
                 std::logic_error);
    tcp::acceptor twice(t.net, t.b);
    twice.bind(9002);
    twice.listen(8);
    twice.async_accept([](tcp::socket, std::error_code) {});
    EXPECT_THROW(twice.async_accept([](tcp::socket, std::error_code) {}), std::logic_error);
}

// --- std::logic_error: duplicate TCP 4-tuple (network.cxx register_tcp_connection) ---
//
// The old connection is still registered while it sits in FIN_WAIT_2:
// its port claim was released by close(), so a new socket can bind the
// same port, but the 4-tuple collides and the connect throws. Commit 2
// (the FIN_WAIT_2 timeout) is the positive side: past 60s the same
// sequence succeeds.

TEST(ErrorsTest, DuplicateFourTupleThrows) {
    TcpPair p;
    tcp::acceptor acceptor(p.net, p.b);
    acceptor.bind(9000);
    acceptor.listen(8);
    tcp::socket accepted(p.net, p.b);
    std::error_code accept_ec{1, std::generic_category()};
    std::error_code connect_ec{1, std::generic_category()};
    tcp::socket client = connect_client(p, acceptor, accepted, accept_ec, connect_ec);
    p.net.run_until(200ms);
    ASSERT_FALSE(connect_ec);
    ASSERT_FALSE(accept_ec);
    const std::uint16_t port = client.local_endpoint().port;
    client.close();
    p.net.run_until(500ms);  // server ACKs the FIN: client is in FIN_WAIT_2
    tcp::socket second(p.net, p.a);
    second.bind(port);
    EXPECT_THROW(second.async_connect(tcp::endpoint{p.b.id(), 9000},
                                      [](std::error_code) {}),
                 std::logic_error);
}

// --- std::logic_error: ephemeral port exhaustion (network.cxx) ---

TEST(ErrorsTest, UdpEphemeralExhaustionThrows) {
    TwoNodes t;
    std::vector<udp::socket> sockets;
    sockets.reserve(16384);
    for (int i = 0; i < 16384; ++i) {
        sockets.emplace_back(t.net, t.a);
        sockets.back().bind(0);
    }
    udp::socket extra(t.net, t.a);
    EXPECT_THROW(extra.bind(0), std::logic_error);
    // An explicit low port is still free: only the ephemeral range is full.
    EXPECT_NO_THROW(extra.bind(8080));
}

TEST(ErrorsTest, TcpEphemeralExhaustionThrows) {
    TwoNodes t;
    std::vector<tcp::socket> sockets;
    sockets.reserve(16384);
    for (int i = 0; i < 16384; ++i) {
        sockets.emplace_back(t.net, t.a);
        sockets.back().bind(0);
    }
    tcp::socket extra(t.net, t.a);
    EXPECT_THROW(extra.bind(0), std::logic_error);
    EXPECT_NO_THROW(extra.bind(9000));
}
