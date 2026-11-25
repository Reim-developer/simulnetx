#include <array>
#include <chrono>
#include <cstddef>
#include <cstring>
#include <functional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

#include <gtest/gtest.h>

#include "simulnetx/network.hxx"
#include "simulnetx/trace.hxx"
#include "simulnetx/udp.hxx"

namespace {

using namespace std::chrono_literals;

using simulnetx::event_kind;
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

std::string as_string(const std::byte* data, std::size_t n) {
    std::string out;
    out.resize(n);
    if (n > 0) {
        std::memcpy(out.data(), data, n);
    }
    return out;
}

TEST(SocketTest, EndpointEqualityAndToString) {
    EXPECT_EQ((endpoint{3, 8080}), (endpoint{3, 8080}));
    EXPECT_NE((endpoint{3, 8080}), (endpoint{3, 8081}));
    EXPECT_NE((endpoint{3, 8080}), (endpoint{4, 8080}));
    EXPECT_EQ(simulnetx::udp::to_string(endpoint{3, 8080}), "node#3:8080");
    EXPECT_EQ(simulnetx::udp::to_string(endpoint{0, 0}), "node#0:0");
}

TEST(SocketTest, BindAndLocalEndpoint) {
    simulnetx::network net(42);
    auto& a = net.add_node("a");
    socket s(net, a);
    EXPECT_TRUE(s.is_open());
    EXPECT_THROW(s.local_endpoint(), std::logic_error);
    s.bind(8080);
    EXPECT_EQ(s.local_endpoint(), (endpoint{a.id(), 8080}));
}

TEST(SocketTest, BindConflictOnSameNodeThrows) {
    simulnetx::network net(42);
    auto& a = net.add_node("a");
    socket s1(net, a);
    s1.bind(8080);
    socket s2(net, a);
    EXPECT_THROW(s2.bind(8080), std::logic_error);
}

TEST(SocketTest, SamePortOnDifferentNodesIsFine) {
    simulnetx::network net(42);
    auto& a = net.add_node("a");
    auto& b = net.add_node("b");
    socket s1(net, a);
    socket s2(net, b);
    s1.bind(8080);
    EXPECT_NO_THROW(s2.bind(8080));
    EXPECT_EQ(s2.local_endpoint(), (endpoint{b.id(), 8080}));
}

TEST(SocketTest, DoubleBindThrows) {
    simulnetx::network net(42);
    auto& a = net.add_node("a");
    socket s(net, a);
    s.bind(8080);
    EXPECT_THROW(s.bind(9090), std::logic_error);
    EXPECT_THROW(s.bind(8080), std::logic_error);
    EXPECT_EQ(s.local_endpoint().port, 8080);
}

TEST(SocketTest, EphemeralPortsArePerNodeFrom49152) {
    simulnetx::network net(42);
    auto& a = net.add_node("a");
    auto& b = net.add_node("b");
    socket e1(net, a);
    socket e2(net, a);
    socket e3(net, b);
    e1.bind(0);
    e2.bind(0);
    e3.bind(0);
    EXPECT_EQ(e1.local_endpoint().port, 49152);
    EXPECT_EQ(e2.local_endpoint().port, 49153);
    EXPECT_EQ(e3.local_endpoint().port, 49152);
}

TEST(SocketTest, CloseUnregistersAndIsIdempotent) {
    simulnetx::network net(42);
    auto& a = net.add_node("a");
    socket s(net, a);
    s.bind(8080);
    s.close();
    EXPECT_FALSE(s.is_open());
    EXPECT_NO_THROW(s.close());
    EXPECT_THROW(s.bind(9090), std::logic_error);
    // The port is free again: a new socket can bind it.
    socket s2(net, a);
    EXPECT_NO_THROW(s2.bind(8080));
}

TEST(SocketTest, DestructorUnregisters) {
    simulnetx::network net(42);
    auto& a = net.add_node("a");
    {
        socket s(net, a);
        s.bind(8080);
    }
    socket s2(net, a);
    EXPECT_NO_THROW(s2.bind(8080));
}

TEST(SocketTest, MoveTransfersBinding) {
    simulnetx::network net(42);
    auto& a = net.add_node("a");
    socket orig(net, a);
    orig.bind(8080);
    socket moved(std::move(orig));
    EXPECT_FALSE(orig.is_open());
    EXPECT_TRUE(moved.is_open());
    EXPECT_EQ(moved.local_endpoint(), (endpoint{a.id(), 8080}));
    // The registry entry moved with the socket: the port is still taken.
    socket other(net, a);
    EXPECT_THROW(other.bind(8080), std::logic_error);
    // And closing the moved socket frees it.
    moved.close();
    EXPECT_NO_THROW(other.bind(8080));
}

TEST(SocketTest, MoveAssignmentTransfersBinding) {
    simulnetx::network net(42);
    auto& a = net.add_node("a");
    socket orig(net, a);
    orig.bind(8080);
    socket target(net, a);
    target.bind(9090);
    target = std::move(orig);
    EXPECT_FALSE(orig.is_open());
    EXPECT_EQ(target.local_endpoint(), (endpoint{a.id(), 8080}));
    // The target's old binding was released by the assignment.
    socket other(net, a);
    EXPECT_NO_THROW(other.bind(9090));
    EXPECT_THROW(other.bind(8080), std::logic_error);
}

TEST(SocketTest, MoveOfUnboundSocket) {
    simulnetx::network net(42);
    auto& a = net.add_node("a");
    socket orig(net, a);
    socket moved(std::move(orig));
    EXPECT_FALSE(orig.is_open());
    EXPECT_TRUE(moved.is_open());
    EXPECT_THROW(moved.local_endpoint(), std::logic_error);
    EXPECT_NO_THROW(moved.bind(8080));
}

// --- async send/receive (commit 2) ---

TEST(SocketTest, EchoExactTrace) {
    simulnetx::network net(42);
    auto& client_node = net.add_node("client");
    auto& server_node = net.add_node("server");
    net.link(client_node.id(), server_node.id(), {.latency = 20ms});

    socket server_sock(net, server_node);
    server_sock.bind(8080);
    std::array<std::byte, 64> server_buf{};
    server_sock.async_receive_from(
        server_buf, [&](std::error_code ec, std::size_t n, endpoint from) {
            EXPECT_FALSE(ec);
            EXPECT_EQ(n, 4u);
            EXPECT_EQ(as_string(server_buf.data(), n), "ping");
            server_sock.async_send_to(as_bytes("pong"), from, [](auto, auto) {});
        });

    socket client_sock(net, client_node);
    std::array<std::byte, 64> client_buf{};
    std::string pong;
    bool send_done = false;
    // Send FIRST, then arm the receive: the client socket is unbound
    // until its first send, and a receive on an unbound socket throws.
    client_sock.async_send_to(as_bytes("ping"), {server_node.id(), 8080},
                              [&](std::error_code ec, std::size_t n) {
                                  EXPECT_FALSE(ec);
                                  EXPECT_EQ(n, 4u);
                                  send_done = true;
                              });
    EXPECT_FALSE(send_done);
    client_sock.async_receive_from(
        client_buf, [&](std::error_code ec, std::size_t n, endpoint /*from*/) {
            EXPECT_FALSE(ec);
            pong = as_string(client_buf.data(), n);
        });

    net.run_until(1s);
    EXPECT_TRUE(send_done);
    EXPECT_EQ(pong, "pong");
    EXPECT_GE(client_sock.local_endpoint().port, 49152);
    EXPECT_EQ(net.dump_trace(),
              "trace: 4 events (seed 42)\n"
              "t=0ns client -> server sent 28 bytes [tag=5828304564438402943]\n"
              "t=20000000ns client -> server delivered 28 bytes [tag=5828304564438402943]\n"
              "t=20000000ns server -> client sent 28 bytes [tag=5828304564438402943]\n"
              "t=40000000ns server -> client delivered 28 bytes [tag=5828304564438402943]\n");
}

TEST(SocketTest, SendCompletionFiresOnlyInsideRunUntil) {
    simulnetx::network net(42);
    auto& a = net.add_node("a");
    auto& b = net.add_node("b");
    net.link(a.id(), b.id(), {.latency = 10ms});
    socket s(net, a);
    bool fired = false;
    s.async_send_to(as_bytes("x"), {b.id(), 8000},
                    [&](std::error_code ec, std::size_t n) {
                        EXPECT_FALSE(ec);
                        EXPECT_EQ(n, 1u);
                        fired = true;
                    });
    EXPECT_FALSE(fired);
    net.run_until(0s);
    EXPECT_TRUE(fired);
}

TEST(SocketTest, ImplicitEphemeralBindOnSend) {
    simulnetx::network net(42);
    auto& a = net.add_node("a");
    auto& b = net.add_node("b");
    net.link(a.id(), b.id(), {.latency = 10ms});
    socket s(net, a);
    EXPECT_THROW(s.local_endpoint(), std::logic_error);
    s.async_send_to(as_bytes("x"), {b.id(), 8000}, [](auto, auto) {});
    EXPECT_EQ(s.local_endpoint().port, 49152);
}

TEST(SocketTest, ReceiveOnUnboundSocketThrows) {
    simulnetx::network net(42);
    auto& a = net.add_node("a");
    socket s(net, a);
    std::array<std::byte, 64> buf{};
    EXPECT_THROW(s.async_receive_from(buf, [](auto, auto, auto) {}), std::logic_error);
}

TEST(SocketTest, SecondPendingReceiveThrows) {
    simulnetx::network net(42);
    auto& a = net.add_node("a");
    socket s(net, a);
    s.bind(8000);
    std::array<std::byte, 64> buf{};
    s.async_receive_from(buf, [](auto, auto, auto) {});
    EXPECT_THROW(s.async_receive_from(buf, [](auto, auto, auto) {}), std::logic_error);
}

TEST(SocketTest, SendToUnknownNodeThrowsAndHandlerNeverFires) {
    simulnetx::network net(42);
    auto& a = net.add_node("a");
    socket s(net, a);
    bool fired = false;
    EXPECT_THROW(s.async_send_to(as_bytes("x"), {99, 8000},
                                 [&](auto, auto) { fired = true; }),
                 std::invalid_argument);
    net.run_until(1s);
    EXPECT_FALSE(fired);
}

TEST(SocketTest, SendWithNoLinkThrows) {
    simulnetx::network net(42);
    auto& a = net.add_node("a");
    auto& b = net.add_node("b");
    socket s(net, a);
    EXPECT_THROW(s.async_send_to(as_bytes("x"), {b.id(), 8000}, [](auto, auto) {}),
                 std::invalid_argument);
}

TEST(SocketTest, AsyncSendUnconnectedThrows) {
    simulnetx::network net(42);
    auto& a = net.add_node("a");
    socket s(net, a);
    EXPECT_THROW(s.async_send(as_bytes("x"), [](auto, auto) {}), std::logic_error);
}

TEST(SocketTest, QueueIsFifoAndReceiveTruncates) {
    simulnetx::network net(42);
    auto& a = net.add_node("a");
    auto& b = net.add_node("b");
    net.link(a.id(), b.id(), {.latency = 10ms});
    socket recv_sock(net, b);
    recv_sock.bind(8000);
    socket send_sock(net, a);
    send_sock.async_send_to(as_bytes("one"), {b.id(), 8000}, [](auto, auto) {});
    send_sock.async_send_to(as_bytes("two-long-message"), {b.id(), 8000},
                            [](auto, auto) {});
    net.run_until(1s);  // both queued, no pending receive
    std::array<std::byte, 64> buf{};
    std::string first;
    recv_sock.async_receive_from(buf, [&](std::error_code ec, std::size_t n, endpoint) {
        EXPECT_FALSE(ec);
        EXPECT_EQ(n, 3u);
        first = as_string(buf.data(), n);
    });
    net.run_until(2s);
    EXPECT_EQ(first, "one");
    std::array<std::byte, 4> small{};
    std::string second;
    std::size_t second_n = 0;
    recv_sock.async_receive_from(small, [&](std::error_code ec, std::size_t n, endpoint) {
        EXPECT_FALSE(ec);
        second_n = n;
        second = as_string(small.data(), n);
    });
    net.run_until(3s);
    EXPECT_EQ(second_n, 4u);
    EXPECT_EQ(second, "two-");
}

TEST(SocketTest, QueuedDatagramCompletesViaDeferredHandler) {
    simulnetx::network net(42);
    auto& a = net.add_node("a");
    auto& b = net.add_node("b");
    net.link(a.id(), b.id(), {.latency = 10ms});
    socket recv_sock(net, b);
    recv_sock.bind(8000);
    socket send_sock(net, a);
    send_sock.async_send_to(as_bytes("queued"), {b.id(), 8000}, [](auto, auto) {});
    net.run_until(1s);  // queued, no pending receive
    std::array<std::byte, 64> buf{};
    bool fired = false;
    recv_sock.async_receive_from(buf, [&](std::error_code ec, std::size_t n, endpoint) {
        EXPECT_FALSE(ec);
        EXPECT_EQ(n, 6u);
        fired = true;
    });
    EXPECT_FALSE(fired);  // never synchronously, even from the queue
    net.run_until(2s);
    EXPECT_TRUE(fired);
}

TEST(SocketTest, ConnectedUdpRoundTrip) {
    simulnetx::network net(42);
    auto& a = net.add_node("a");
    auto& b = net.add_node("b");
    net.link(a.id(), b.id(), {.latency = 10ms});
    socket server(net, b);
    server.bind(8000);
    socket client(net, a);
    client.connect({b.id(), 8000});
    EXPECT_TRUE(client.is_connected());
    std::array<std::byte, 64> buf{};
    std::string got;
    server.async_receive_from(buf, [&](std::error_code ec, std::size_t n, endpoint from) {
        EXPECT_FALSE(ec);
        got = as_string(buf.data(), n);
        server.async_send_to(as_bytes("ack"), from, [](auto, auto) {});
    });
    std::string ack;
    client.async_send(as_bytes("hi"), [](auto, auto) {});
    client.async_receive(buf, [&](std::error_code ec, std::size_t n) {
        EXPECT_FALSE(ec);
        ack = as_string(buf.data(), n);
    });
    net.run_until(1s);
    EXPECT_EQ(got, "hi");
    EXPECT_EQ(ack, "ack");
}

TEST(SocketTest, ReconnectReplacesPeer) {
    simulnetx::network net(42);
    auto& a = net.add_node("a");
    auto& b = net.add_node("b");
    auto& c = net.add_node("c");
    net.link(a.id(), b.id(), {.latency = 10ms});
    net.link(a.id(), c.id(), {.latency = 10ms});
    socket server_b(net, b);
    server_b.bind(8000);
    socket server_c(net, c);
    server_c.bind(8000);
    std::array<std::byte, 64> buf_b{};
    std::array<std::byte, 64> buf_c{};
    std::string got_b;
    std::string got_c;
    server_b.async_receive_from(buf_b, [&](std::error_code ec, std::size_t n, endpoint) {
        EXPECT_FALSE(ec);
        got_b = as_string(buf_b.data(), n);
    });
    server_c.async_receive_from(buf_c, [&](std::error_code ec, std::size_t n, endpoint) {
        EXPECT_FALSE(ec);
        got_c = as_string(buf_c.data(), n);
    });
    socket client(net, a);
    client.connect({b.id(), 8000});
    client.async_send(as_bytes("one"), [](auto, auto) {});
    client.connect({c.id(), 8000});
    client.async_send(as_bytes("two"), [](auto, auto) {});
    net.run_until(1s);
    EXPECT_EQ(got_b, "one");
    EXPECT_EQ(got_c, "two");
}

TEST(SocketTest, UnboundPortSilentDropInStrictMode) {
    simulnetx::network net(42);
    auto& a = net.add_node("a");
    auto& b = net.add_node("b");
    net.link(a.id(), b.id(), {.latency = 10ms});
    EXPECT_TRUE(net.strict_tags());
    socket s(net, a);
    s.async_send_to(as_bytes("nobody"), {b.id(), 9999}, [](auto, auto) {});
    EXPECT_NO_THROW(net.run_until(1s));
    net.delivered_count().equal(1);
    net.dropped_count().equal(0);
}

TEST(SocketTest, DupDeliversTwice) {
    simulnetx::network net(11);
    auto& a = net.add_node("a");
    auto& b = net.add_node("b");
    net.link(a.id(), b.id(), {.latency = 10ms, .dup_rate = 1.0});
    socket recv_sock(net, b);
    recv_sock.bind(8000);
    std::array<std::byte, 64> buf{};
    int received = 0;
    std::function<void(std::error_code, std::size_t, endpoint)> arm;
    arm = [&](std::error_code ec, std::size_t, endpoint) {
        if (!ec) {
            ++received;
            recv_sock.async_receive_from(buf, arm);
        }
    };
    recv_sock.async_receive_from(buf, arm);
    socket send_sock(net, a);
    send_sock.async_send_to(as_bytes("twice"), {b.id(), 8000}, [](auto, auto) {});
    net.run_until(1s);
    net.sent_count().equal(1);
    net.delivered_count().equal(2);
    EXPECT_EQ(received, 2);
}

TEST(SocketTest, SelfSendDeliversAtCurrentTime) {
    simulnetx::network net(42);
    auto& a = net.add_node("a");
    socket s(net, a);
    s.bind(8000);
    std::array<std::byte, 64> buf{};
    std::string got;
    s.async_receive_from(buf, [&](std::error_code ec, std::size_t n, endpoint) {
        EXPECT_FALSE(ec);
        got = as_string(buf.data(), n);
    });
    s.async_send_to(as_bytes("self"), {a.id(), 8000}, [](auto, auto) {});
    net.run_until(1s);
    EXPECT_EQ(got, "self");
    ASSERT_EQ(net.trace().size(), 2u);
    EXPECT_EQ(net.trace()[1].kind, event_kind::delivered);
    EXPECT_EQ(net.trace()[1].at, 0ns);
}

TEST(SocketTest, ZeroLengthDatagram) {
    simulnetx::network net(42);
    auto& a = net.add_node("a");
    auto& b = net.add_node("b");
    net.link(a.id(), b.id(), {.latency = 10ms});
    socket recv_sock(net, b);
    recv_sock.bind(8000);
    std::array<std::byte, 64> buf{};
    bool fired = false;
    std::size_t got_n = 99;
    recv_sock.async_receive_from(buf, [&](std::error_code ec, std::size_t n, endpoint) {
        EXPECT_FALSE(ec);
        got_n = n;
        fired = true;
    });
    socket send_sock(net, a);
    send_sock.async_send_to({}, {b.id(), 8000}, [](auto, auto) {});
    net.run_until(1s);
    EXPECT_TRUE(fired);
    EXPECT_EQ(got_n, 0u);
}

TEST(SocketTest, LossCountsMatchReceived) {
    simulnetx::network net(42);
    auto& a = net.add_node("a");
    auto& b = net.add_node("b");
    net.link(a.id(), b.id(), {.latency = 10ms, .loss_rate = 0.3});
    socket recv_sock(net, b);
    recv_sock.bind(9000);
    std::array<std::byte, 64> buf{};
    int received = 0;
    std::function<void(std::error_code, std::size_t, endpoint)> arm;
    arm = [&](std::error_code ec, std::size_t, endpoint) {
        if (!ec) {
            ++received;
            recv_sock.async_receive_from(buf, arm);
        }
    };
    recv_sock.async_receive_from(buf, arm);
    socket send_sock(net, a);
    for (int i = 0; i < 5; ++i) {
        send_sock.async_send_to(as_bytes("x"), {b.id(), 9000}, [](auto, auto) {});
    }
    net.run_until(1s);
    net.sent_count().equal(5);
    net.delivered_count().equal(2);
    net.dropped_count().equal(3);
    EXPECT_EQ(received, 2);
    net.event_count(event_kind::dropped).equal(3);
}

TEST(SocketTest, DeterminismIdenticalTraceAndReceives) {
    auto run = [] {
        simulnetx::network net(42);
        auto& a = net.add_node("a");
        auto& b = net.add_node("b");
        net.link(a.id(), b.id(), {.latency = 10ms, .jitter = 5ms, .loss_rate = 0.2});
        socket recv_sock(net, b);
        recv_sock.bind(8000);
        std::array<std::byte, 64> buf{};
        std::vector<std::string> got;
        std::function<void(std::error_code, std::size_t, endpoint)> arm;
        arm = [&](std::error_code ec, std::size_t n, endpoint) {
            if (!ec) {
                got.push_back(as_string(buf.data(), n));
                recv_sock.async_receive_from(buf, arm);
            }
        };
        recv_sock.async_receive_from(buf, arm);
        socket send_sock(net, a);
        for (int i = 0; i < 10; ++i) {
            send_sock.async_send_to(as_bytes("m" + std::to_string(i)), {b.id(), 8000},
                                    [](auto, auto) {});
        }
        net.run_until(5s);
        return std::pair{net.dump_trace(), got};
    };
    const auto [trace1, got1] = run();
    const auto [trace2, got2] = run();
    EXPECT_EQ(trace1, trace2);
    EXPECT_EQ(got1, got2);
    EXPECT_EQ(got1.size(), 9u);
}

TEST(SocketTest, TransparencyTracingOnOffIdentical) {
    auto run = [](bool tracing) {
        simulnetx::network net(42);
        net.set_tracing(tracing);
        auto& a = net.add_node("a");
        auto& b = net.add_node("b");
        net.link(a.id(), b.id(), {.latency = 10ms, .loss_rate = 0.3});
        socket recv_sock(net, b);
        recv_sock.bind(8000);
        std::array<std::byte, 64> buf{};
        int received = 0;
        std::function<void(std::error_code, std::size_t, endpoint)> arm;
        arm = [&](std::error_code ec, std::size_t, endpoint) {
            if (!ec) {
                ++received;
                recv_sock.async_receive_from(buf, arm);
            }
        };
        recv_sock.async_receive_from(buf, arm);
        socket send_sock(net, a);
        for (int i = 0; i < 10; ++i) {
            send_sock.async_send_to(as_bytes("m"), {b.id(), 8000}, [](auto, auto) {});
        }
        net.run_until(5s);
        return std::pair{static_cast<std::size_t>(net.sent_count()), received};
    };
    EXPECT_EQ(run(true), run(false));
}

TEST(SocketTest, MoveTransfersPendingReceive) {
    simulnetx::network net(42);
    auto& a = net.add_node("a");
    auto& b = net.add_node("b");
    net.link(a.id(), b.id(), {.latency = 10ms});
    socket orig(net, b);
    orig.bind(8000);
    std::array<std::byte, 64> buf{};
    std::string got;
    orig.async_receive_from(buf, [&](std::error_code ec, std::size_t n, endpoint) {
        EXPECT_FALSE(ec);
        got = as_string(buf.data(), n);
    });
    socket moved(std::move(orig));
    socket send_sock(net, a);
    send_sock.async_send_to(as_bytes("moved"), {b.id(), 8000}, [](auto, auto) {});
    net.run_until(1s);
    EXPECT_EQ(got, "moved");
}

TEST(SocketTest, MoveTransfersQueuedDatagram) {
    simulnetx::network net(42);
    auto& a = net.add_node("a");
    auto& b = net.add_node("b");
    net.link(a.id(), b.id(), {.latency = 10ms});
    socket orig(net, b);
    orig.bind(8000);
    socket send_sock(net, a);
    send_sock.async_send_to(as_bytes("moved"), {b.id(), 8000}, [](auto, auto) {});
    net.run_until(1s);  // queued on orig
    socket moved(std::move(orig));
    std::array<std::byte, 64> buf{};
    std::string got;
    moved.async_receive_from(buf, [&](std::error_code ec, std::size_t n, endpoint) {
        EXPECT_FALSE(ec);
        got = as_string(buf.data(), n);
    });
    net.run_until(2s);
    EXPECT_EQ(got, "moved");
}

// --- cancel + crash semantics (commit 3) ---

TEST(SocketTest, CancelPendingReceiveReportsCanceled) {
    simulnetx::network net(42);
    auto& a = net.add_node("a");
    socket s(net, a);
    s.bind(8000);
    std::array<std::byte, 64> buf{};
    std::error_code got;
    bool fired = false;
    s.async_receive_from(buf, [&](std::error_code ec, std::size_t n, endpoint) {
        got = ec;
        EXPECT_EQ(n, 0u);
        fired = true;
    });
    s.cancel();
    EXPECT_FALSE(fired);  // never synchronously
    net.run_until(1s);
    EXPECT_TRUE(fired);
    EXPECT_EQ(got, std::errc::operation_canceled);
}

TEST(SocketTest, CancelWithNoPendingReceiveIsNoop) {
    simulnetx::network net(42);
    auto& a = net.add_node("a");
    socket s(net, a);
    s.bind(8000);
    EXPECT_NO_THROW(s.cancel());
    net.run_until(1s);  // nothing scheduled, nothing fires
}

TEST(SocketTest, CloseCancelsPendingReceive) {
    simulnetx::network net(42);
    auto& a = net.add_node("a");
    socket s(net, a);
    s.bind(8000);
    std::array<std::byte, 64> buf{};
    std::error_code got;
    bool fired = false;
    s.async_receive_from(buf, [&](std::error_code ec, std::size_t, endpoint) {
        got = ec;
        fired = true;
    });
    s.close();
    EXPECT_FALSE(fired);
    net.run_until(1s);
    EXPECT_TRUE(fired);
    EXPECT_EQ(got, std::errc::operation_canceled);
}

TEST(SocketTest, CrashCancelsPendingReceive) {
    simulnetx::network net(42);
    auto& a = net.add_node("a");
    auto& b = net.add_node("b");
    net.link(a.id(), b.id(), {.latency = 10ms});
    socket recv_sock(net, b);
    recv_sock.bind(8000);
    std::array<std::byte, 64> buf{};
    bool fired = false;
    std::error_code got;
    recv_sock.async_receive_from(buf, [&](std::error_code ec, std::size_t, endpoint) {
        fired = true;
        got = ec;
    });
    net.crash(b.id());
    EXPECT_FALSE(fired);  // never synchronously
    net.run_until(1s);
    EXPECT_TRUE(fired);
    EXPECT_EQ(got, std::errc::operation_canceled);
}

TEST(SocketTest, CrashDropsQueueButBindingSurvives) {
    simulnetx::network net(42);
    auto& a = net.add_node("a");
    auto& b = net.add_node("b");
    net.link(a.id(), b.id(), {.latency = 10ms});
    socket recv_sock(net, b);
    recv_sock.bind(8000);
    socket send_sock(net, a);
    send_sock.async_send_to(as_bytes("gone"), {b.id(), 8000}, [](auto, auto) {});
    net.run_until(100ms);  // "gone" queued (no pending receive)
    net.crash(b.id());
    net.restart(b.id());
    EXPECT_TRUE(recv_sock.is_open());
    EXPECT_EQ(recv_sock.local_endpoint().port, 8000);
    // The queued datagram was dropped: arm a receive, nothing comes.
    std::array<std::byte, 64> buf{};
    bool fired = false;
    recv_sock.async_receive_from(buf, [&](std::error_code ec, std::size_t, endpoint) {
        EXPECT_FALSE(ec);
        fired = true;
    });
    net.run_until(500ms);
    EXPECT_FALSE(fired);
    // And a post-restart datagram IS received on the same binding.
    send_sock.async_send_to(as_bytes("back"), {b.id(), 8000}, [](auto, auto) {});
    net.run_until(1s);
    EXPECT_TRUE(fired);
}

TEST(SocketTest, CrashSendRules) {
    simulnetx::network net(42);
    auto& a = net.add_node("a");
    auto& b = net.add_node("b");
    net.link(a.id(), b.id(), {.latency = 10ms});
    socket recv_sock(net, b);
    recv_sock.bind(8000);
    socket send_sock(net, a);
    // A crashed node cannot send: throws synchronously, handler never fires.
    net.crash(a.id());
    bool fired = false;
    EXPECT_THROW(send_sock.async_send_to(as_bytes("x"), {b.id(), 8000},
                                         [&](auto, auto) { fired = true; }),
                 std::logic_error);
    net.run_until(1s);
    EXPECT_FALSE(fired);
    // A message TO a crashed node is dropped (crashed), at send time.
    net.restart(a.id());
    net.crash(b.id());
    send_sock.async_send_to(as_bytes("x"), {b.id(), 8000}, [](auto, auto) {});
    net.run_until(2s);
    net.dropped_count().equal(1);
    // crashed(a), restarted(a), crashed(b), sent, dropped(crashed).
    ASSERT_EQ(net.trace().size(), 5u);
    EXPECT_EQ(net.trace()[3].kind, event_kind::sent);
    EXPECT_EQ(net.trace()[4].kind, event_kind::dropped);
    EXPECT_EQ(net.trace()[4].reason, simulnetx::drop_reason::crashed);
}

}  // namespace
