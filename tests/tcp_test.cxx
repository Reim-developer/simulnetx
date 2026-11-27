#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cstring>
#include <sstream>
#include <string>
#include <vector>

#include "simulnetx/network.hxx"
#include "simulnetx/tcp.hxx"

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

std::string from_bytes(const std::vector<std::byte>& bytes) {
    std::string text;
    text.reserve(bytes.size());
    for (const std::byte b : bytes) {
        text.push_back(static_cast<char>(b));
    }
    return text;
}

}  // namespace

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

TEST(TcpTest, BacklogOverflow) {
    TcpPair p;
    tcp::acceptor acceptor(p.net, p.b);
    acceptor.bind(9000);
    acceptor.listen(1);
    // No accept is armed yet: the first client's completed handshake
    // sits in the completed queue (backlog 1).
    tcp::socket first(p.net, p.a);
    std::error_code first_ec{1, std::generic_category()};
    first.async_connect(tcp::endpoint{p.b.id(), 9000},
                        [&first_ec](std::error_code ec) { first_ec = ec; });
    p.net.run_until(200ms);
    EXPECT_FALSE(first_ec);  // handshake done, socket in the completed queue

    // The second client connects only now, while the queue is full:
    // its SYN is dropped, and so are its retransmits.
    tcp::socket second(p.net, p.a);
    std::error_code second_ec{1, std::generic_category()};
    second.async_connect(tcp::endpoint{p.b.id(), 9000},
                         [&second_ec](std::error_code ec) { second_ec = ec; });

    p.net.run_until(15s);
    EXPECT_EQ(second_ec, std::make_error_code(std::errc::timed_out));

    // Arming the accept now drains the completed queue.
    tcp::socket accepted(p.net, p.b);
    std::error_code accept_ec{1, std::generic_category()};
    acceptor.async_accept([&accepted, &accept_ec](tcp::socket s, std::error_code ec) {
        accepted = std::move(s);
        accept_ec = ec;
    });
    p.net.run_until(16s);
    EXPECT_FALSE(accept_ec);
    EXPECT_EQ(accepted.remote_endpoint(), (tcp::endpoint{p.a.id(), 49152}));
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

TEST(TcpTest, LossRetransmit) {
    TcpPair p(42);
    p.net.link(p.a.id(), p.b.id(),
               link_config{.latency = 20ms, .loss_rate = 0.3});
    TcpServer server(p);
    tcp::socket accepted(p.net, p.b);
    std::error_code accept_ec{1, std::generic_category()};
    std::error_code connect_ec{1, std::generic_category()};
    tcp::socket client = connect_client(p, server, accepted, accept_ec, connect_ec);
    p.net.run_until(2s);
    ASSERT_FALSE(connect_ec);
    ASSERT_FALSE(accept_ec);

    std::array<std::byte, 16> buf{};
    std::error_code read_ec{1, std::generic_category()};
    std::size_t read_n = 99;
    accepted.async_read_some(buf, [&](std::error_code ec, std::size_t n) {
        read_ec = ec;
        read_n = n;
    });
    client.async_write("hello", [](std::error_code, std::size_t) {});
    p.net.run_until(5s);
    EXPECT_FALSE(read_ec);
    EXPECT_EQ(read_n, 5u);
    EXPECT_EQ(from_bytes({buf.begin(), buf.begin() + 5}), "hello");
    // Some segment was lost and retransmitted: more sent than delivered.
    EXPECT_GT(static_cast<std::size_t>(p.net.sent_count()), static_cast<std::size_t>(p.net.delivered_count()));
    EXPECT_GT(static_cast<std::size_t>(p.net.dropped_count()), 0u);
}

TEST(TcpTest, ReorderReassembly) {
    TcpPair p(13);
    p.net.link(p.a.id(), p.b.id(),
               link_config{.latency = 20ms, .jitter = 5ms, .reorder_rate = 1.0});
    TcpServer server(p);
    tcp::socket accepted(p.net, p.b);
    std::error_code accept_ec{1, std::generic_category()};
    std::error_code connect_ec{1, std::generic_category()};
    tcp::socket client = connect_client(p, server, accepted, accept_ec, connect_ec);
    p.net.run_until(2s);
    ASSERT_FALSE(connect_ec);
    ASSERT_FALSE(accept_ec);

    const std::string payload = [] {
        std::string s;
        for (int i = 0; i < 3000; ++i) {
            s.push_back(static_cast<char>('a' + (i % 26)));
        }
        return s;
    }();
    client.async_write(payload, [](std::error_code, std::size_t) {});
    std::vector<std::byte> received;
    std::array<std::byte, 512> buf{};
    std::function<void(std::error_code, std::size_t)> read_more =
        [&](std::error_code ec, std::size_t n) {
            if (ec || n == 0) {
                return;
            }
            received.insert(received.end(), buf.begin(), buf.begin() + n);
            if (received.size() < 3000) {
                accepted.async_read_some(buf, read_more);
            }
        };
    accepted.async_read_some(buf, read_more);
    p.net.run_until(5s);
    EXPECT_EQ(received.size(), 3000u);
    EXPECT_EQ(from_bytes(received), payload);
}

TEST(TcpTest, DupDeduped) {
    TcpPair p(11);
    p.net.link(p.a.id(), p.b.id(),
               link_config{.latency = 20ms, .dup_rate = 1.0});
    TcpServer server(p);
    tcp::socket accepted(p.net, p.b);
    std::error_code accept_ec{1, std::generic_category()};
    std::error_code connect_ec{1, std::generic_category()};
    tcp::socket client = connect_client(p, server, accepted, accept_ec, connect_ec);
    p.net.run_until(2s);
    ASSERT_FALSE(connect_ec);
    ASSERT_FALSE(accept_ec);

    client.async_write("hello", [](std::error_code, std::size_t) {});
    std::array<std::byte, 16> buf{};
    std::size_t read_n = 99;
    accepted.async_read_some(buf, [&](std::error_code ec, std::size_t n) {
        EXPECT_FALSE(ec);
        read_n = n;
    });
    p.net.run_until(3s);
    EXPECT_EQ(read_n, 5u);
    EXPECT_EQ(from_bytes({buf.begin(), buf.begin() + 5}), "hello");
    // Every message was duplicated on the wire, but only 5 bytes read:
    // the duplicate data segment was deduped by sequence number.
    EXPECT_GT(static_cast<std::size_t>(p.net.delivered_count()), static_cast<std::size_t>(p.net.sent_count()));
}

TEST(TcpTest, RtoExhaustion) {
    TcpPair p(7);
    p.net.link(p.a.id(), p.b.id(),
               link_config{.latency = 20ms, .loss_rate = 1.0});
    tcp::socket client(p.net, p.a);
    std::error_code connect_ec{1, std::generic_category()};
    client.async_connect(tcp::endpoint{p.b.id(), 9000},
                         [&connect_ec](std::error_code ec) { connect_ec = ec; });
    // RTO firings: 200ms, 600ms, 1400ms, 3000ms, 6200ms (5 retransmits),
    // then the give-up at 12600ms: 200+400+800+1600+3200+6400. The
    // give-up also sends a best-effort RST, which the lossy link drops.
    p.net.run_until(15s);
    EXPECT_EQ(connect_ec, std::make_error_code(std::errc::timed_out));
    EXPECT_EQ(static_cast<std::size_t>(p.net.sent_count()), 7u);  // 6 SYNs + RST
    EXPECT_EQ(static_cast<std::size_t>(p.net.dropped_count()), 7u);
}

TEST(TcpTest, FinEof) {
    TcpPair p;
    TcpServer server(p);
    tcp::socket accepted(p.net, p.b);
    std::error_code accept_ec{1, std::generic_category()};
    std::error_code connect_ec{1, std::generic_category()};
    tcp::socket client = connect_client(p, server, accepted, accept_ec, connect_ec);
    p.net.run_until(200ms);
    ASSERT_FALSE(connect_ec);

    client.async_write("hi", [](std::error_code, std::size_t) {});
    client.close();  // FIN after the write

    std::array<std::byte, 16> buf{};
    std::size_t first_n = 99;
    std::size_t second_n = 99;
    std::error_code first_ec{1, std::generic_category()};
    std::error_code second_ec{1, std::generic_category()};
    accepted.async_read_some(buf, [&](std::error_code ec, std::size_t n) {
        first_ec = ec;
        first_n = n;
        accepted.async_read_some(buf, [&](std::error_code ec2, std::size_t n2) {
            second_ec = ec2;
            second_n = n2;
        });
    });
    p.net.run_until(500ms);
    EXPECT_FALSE(first_ec);
    EXPECT_EQ(first_n, 2u);
    EXPECT_EQ(from_bytes({buf.begin(), buf.begin() + 2}), "hi");
    EXPECT_FALSE(second_ec);
    EXPECT_EQ(second_n, 0u);  // EOF
}

TEST(TcpTest, HalfClose) {
    TcpPair p;
    TcpServer server(p);
    tcp::socket accepted(p.net, p.b);
    std::error_code accept_ec{1, std::generic_category()};
    std::error_code connect_ec{1, std::generic_category()};
    tcp::socket client = connect_client(p, server, accepted, accept_ec, connect_ec);
    p.net.run_until(200ms);
    ASSERT_FALSE(connect_ec);

    client.async_write("hi", [](std::error_code, std::size_t) {});
    client.close();  // client -> FIN_WAIT_1, still readable

    std::array<std::byte, 16> server_buf{};
    std::array<std::byte, 16> client_buf{};
    std::size_t server_read = 99;
    std::size_t client_read = 99;
    std::size_t client_eof = 99;
    accepted.async_read_some(server_buf, [&](std::error_code ec, std::size_t n) {
        EXPECT_FALSE(ec);
        server_read = n;
        // Server is in CLOSE_WAIT: it can still write back.
        accepted.async_write("bye", [](std::error_code, std::size_t) {});
        accepted.async_read_some(server_buf, [&](std::error_code, std::size_t n2) {
            EXPECT_EQ(n2, 0u);  // EOF after the client's FIN
            accepted.close();   // server's FIN: the client then sees EOF too
        });
    });
    // The client's read is armed AFTER close(): half-close.
    client.async_read_some(client_buf, [&](std::error_code ec, std::size_t n) {
        EXPECT_FALSE(ec);
        client_read = n;
        client.async_read_some(client_buf, [&](std::error_code, std::size_t n2) {
            client_eof = n2;  // EOF after the server's FIN
        });
    });

    p.net.run_until(1s);
    EXPECT_EQ(server_read, 2u);
    EXPECT_EQ(from_bytes({server_buf.begin(), server_buf.begin() + 2}), "hi");
    EXPECT_EQ(client_read, 3u);
    EXPECT_EQ(from_bytes({client_buf.begin(), client_buf.begin() + 3}), "bye");
    EXPECT_EQ(client_eof, 0u);
}

TEST(TcpTest, SimultaneousClose) {
    TcpPair p;
    TcpServer server(p);
    tcp::socket accepted(p.net, p.b);
    std::error_code accept_ec{1, std::generic_category()};
    std::error_code connect_ec{1, std::generic_category()};
    tcp::socket client = connect_client(p, server, accepted, accept_ec, connect_ec);
    p.net.run_until(200ms);
    ASSERT_FALSE(connect_ec);

    client.close();
    accepted.close();
    p.net.run_until(500ms);
    // Both sides are fully closed: a new write is rejected.
    EXPECT_THROW(client.async_write("x", [](std::error_code, std::size_t) {}),
                 std::logic_error);
    EXPECT_THROW(accepted.async_write("x", [](std::error_code, std::size_t) {}),
                 std::logic_error);
}

TEST(TcpTest, CloseCancelsRead) {
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
    accepted.async_read_some(buf, [&read_ec](std::error_code ec, std::size_t) {
        read_ec = ec;
    });
    accepted.close();
    p.net.run_until(500ms);
    EXPECT_EQ(read_ec, std::make_error_code(std::errc::operation_canceled));
}

TEST(TcpTest, RebindAfterClose) {
    TcpPair p;
    tcp::acceptor first(p.net, p.b);
    first.bind(9000);
    first.listen(8);
    first.close();
    // The port is free again immediately (no TIME_WAIT).
    tcp::acceptor second(p.net, p.b);
    EXPECT_NO_THROW(second.bind(9000));
    EXPECT_NO_THROW(second.listen(8));
    tcp::socket accepted(p.net, p.b);
    std::error_code accept_ec{1, std::generic_category()};
    second.async_accept([&accepted, &accept_ec](tcp::socket s, std::error_code ec) {
        accepted = std::move(s);
        accept_ec = ec;
    });
    tcp::socket client(p.net, p.a);
    std::error_code connect_ec{1, std::generic_category()};
    client.async_connect(tcp::endpoint{p.b.id(), 9000},
                         [&connect_ec](std::error_code ec) { connect_ec = ec; });
    p.net.run_until(200ms);
    EXPECT_FALSE(connect_ec);
    EXPECT_FALSE(accept_ec);
}

// FIN_WAIT_2 timeout (v1.0.0): the client closes, the server ACKs the
// FIN but never sends its own FIN, so the client sits in FIN_WAIT_2.
// The port rebinds at once (close() released the claim), but the
// 4-tuple is still registered: a second connect from the same port
// throws logic_error (pinned in ErrorsTest.DuplicateFourTupleThrows).
// After 60s the silent timer frees the 4-tuple, so a third connect
// from the same port registers without throwing. The connect then
// times out: the server's side is still in CLOSE_WAIT and still owns
// its half of the 4-tuple, so the SYN is answered with bare ACKs and
// the handshake cannot complete. That is real-TCP behavior, not a sim
// bug - the timeout frees YOUR side only; it cannot free the peer's.
TEST(TcpTest, FinWait2TimeoutFreesFourTuple) {
    TcpPair p;
    TcpServer server(p);
    tcp::socket accepted(p.net, p.b);
    std::error_code accept_ec{1, std::generic_category()};
    std::error_code connect_ec{1, std::generic_category()};
    tcp::socket client = connect_client(p, server, accepted, accept_ec, connect_ec);
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
    second.close();  // release the port claim (the failed connect did not)

    p.net.run_until(61s);  // past the 60s FIN_WAIT_2 timeout
    tcp::socket third(p.net, p.a);
    third.bind(port);
    std::error_code connect_ec3{1, std::generic_category()};
    EXPECT_NO_THROW(third.async_connect(tcp::endpoint{p.b.id(), 9000},
                                        [&connect_ec3](std::error_code ec) {
                                            connect_ec3 = ec;
                                        }));
    p.net.run_until(75s);  // past the 12.6s RTO exhaustion
    EXPECT_EQ(connect_ec3, std::make_error_code(std::errc::timed_out));
}

TEST(TcpTest, CrashMidTransfer) {
    TcpPair p;
    TcpServer server(p);
    tcp::socket accepted(p.net, p.b);
    std::error_code accept_ec{1, std::generic_category()};
    std::error_code connect_ec{1, std::generic_category()};
    tcp::socket client = connect_client(p, server, accepted, accept_ec, connect_ec);
    p.net.run_until(200ms);
    ASSERT_FALSE(connect_ec);

    std::error_code client_write_ec{1, std::generic_category()};
    client.async_write("hello", [&](std::error_code ec, std::size_t) {
        client_write_ec = ec;
    });
    p.net.run_for(10ms);  // the data is in flight
    p.net.crash(p.b.id());  // the server node dies mid-transfer
    // The peer's crash does not cancel the client's write: the RTO
    // retransmits the data, which is dropped while the server is down.
    // The client's binding survives the peer's crash (it is still
    // established and bound).
    EXPECT_EQ(client.local_endpoint(), (tcp::endpoint{p.a.id(), 49152}));
    p.net.run_until(500ms);
    EXPECT_EQ(client_write_ec.value(), 1);  // still pending
    // After the restart the retransmitted data reaches a server that
    // has no connection for it, so it answers with an RST and the
    // client learns connection_reset.
    p.net.restart(p.b.id());
    p.net.run_until(3s);
    EXPECT_EQ(client_write_ec, std::make_error_code(std::errc::connection_reset));
    // After the restart, a fresh connection works.
    tcp::socket accepted2(p.net, p.b);
    std::error_code accept_ec2{1, std::generic_category()};
    server.acceptor.async_accept(
        [&accepted2, &accept_ec2](tcp::socket s, std::error_code ec) {
            accepted2 = std::move(s);
            accept_ec2 = ec;
        });
    tcp::socket client2(p.net, p.a);
    std::error_code connect_ec2{1, std::generic_category()};
    client2.async_connect(tcp::endpoint{p.b.id(), 9000},
                          [&connect_ec2](std::error_code ec) { connect_ec2 = ec; });
    p.net.run_until(4s);
    EXPECT_FALSE(connect_ec2);
    EXPECT_FALSE(accept_ec2);
}

TEST(TcpTest, CrashCancelsPendingAndBindingSurvives) {
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
    accepted.async_read_some(buf, [&read_ec](std::error_code ec, std::size_t) {
        read_ec = ec;
    });
    p.net.crash(p.b.id());  // the server node crashes with a read pending
    p.net.run_until(500ms);
    EXPECT_EQ(read_ec, std::make_error_code(std::errc::operation_canceled));
    // The acceptor's listener binding survives the restart.
    p.net.restart(p.b.id());
    tcp::socket accepted2(p.net, p.b);
    std::error_code accept_ec2{1, std::generic_category()};
    server.acceptor.async_accept(
        [&accepted2, &accept_ec2](tcp::socket s, std::error_code ec) {
            accepted2 = std::move(s);
            accept_ec2 = ec;
        });
    tcp::socket client2(p.net, p.a);
    std::error_code connect_ec2{1, std::generic_category()};
    client2.async_connect(tcp::endpoint{p.b.id(), 9000},
                          [&connect_ec2](std::error_code ec) { connect_ec2 = ec; });
    p.net.run_until(1s);
    EXPECT_FALSE(connect_ec2);
    EXPECT_FALSE(accept_ec2);
}

TEST(TcpTest, Determinism) {
    auto run_once = [] {
        TcpPair p;
        TcpServer server(p);
        tcp::socket accepted(p.net, p.b);
        std::error_code accept_ec{1, std::generic_category()};
        std::error_code connect_ec{1, std::generic_category()};
        tcp::socket client =
            connect_client(p, server, accepted, accept_ec, connect_ec);
        p.net.run_until(200ms);
        client.async_write("hello", [](std::error_code, std::size_t) {});
        std::array<std::byte, 16> buf{};
        accepted.async_read_some(buf, [](std::error_code, std::size_t) {});
        p.net.run_until(500ms);
        std::ostringstream dump;
        p.net.dump_trace(dump);
        return dump.str();
    };
    EXPECT_EQ(run_once(), run_once());
}

TEST(TcpTest, Transparency) {
    auto stats_with = [](bool tracing) {
        TcpPair p;
        p.net.set_tracing(tracing);
        TcpServer server(p);
        tcp::socket accepted(p.net, p.b);
        std::error_code accept_ec{1, std::generic_category()};
        std::error_code connect_ec{1, std::generic_category()};
        tcp::socket client =
            connect_client(p, server, accepted, accept_ec, connect_ec);
        p.net.run_until(200ms);
        client.async_write("hello", [](std::error_code, std::size_t) {});
        std::array<std::byte, 16> buf{};
        accepted.async_read_some(buf, [](std::error_code, std::size_t) {});
        p.net.run_until(500ms);
        return std::array<std::size_t, 3>{static_cast<std::size_t>(p.net.sent_count()),
                                          static_cast<std::size_t>(p.net.delivered_count()),
                                          static_cast<std::size_t>(p.net.dropped_count())};
    };
    EXPECT_EQ(stats_with(true), stats_with(false));
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

TEST(TcpTest, TraceHasNoTimerFired) {
    TcpPair p;
    TcpServer server(p);
    tcp::socket accepted(p.net, p.b);
    std::error_code accept_ec{1, std::generic_category()};
    std::error_code connect_ec{1, std::generic_category()};
    tcp::socket client = connect_client(p, server, accepted, accept_ec, connect_ec);
    p.net.run_until(200ms);
    client.async_write("hello", [](std::error_code, std::size_t) {});
    std::array<std::byte, 16> buf{};
    accepted.async_read_some(buf, [](std::error_code, std::size_t) {});
    p.net.run_until(500ms);
    // RTO timers ride schedule_silent: no timer_fired events, no
    // timer ids consumed.
    EXPECT_EQ(static_cast<std::size_t>(p.net.event_count(event_kind::timer_fired)), 0u);
}
