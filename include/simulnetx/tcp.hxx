#pragma once

// TCP-like reliable stream over the sim (v0.7.0 design): a byte stream
// with a 3-way handshake, sequence numbers, cumulative ACKs,
// retransmission and FIN close, in the same async Asio-style shape as
// the UDP shim. No congestion control, no flow-control window, no SACK
// - those belong to a real stack, not a sim.
//
// INVARIANT: no completion handler ever runs outside run_until. All
// completions fire as zero-delay sim events (via the network's private
// schedule_silent path, so they leave no timer_fired event and consume
// no timer id); RTO retransmissions fire via schedule_silent_at, the
// delayed twin of schedule_silent, for the same reason.
//
// ERROR SPLIT: the sync API throws like the rest of simulnetx
// (std::invalid_argument for bad arguments, std::logic_error for state
// violations); async completions report via std::error_code. The
// user-initiated sends (async_connect, async_write) throw synchronously
// out of the call when they cannot be performed - exactly like
// node::send - and the handler never fires. Protocol-machinery sends
// (ACKs, SYN+ACKs, retransmits, FINs, RSTs) are best-effort: a failure
// degrades to RTO timeouts instead of aborting run_until. A connection
// needs a bidirectional link: over a directed link the handshake
// cannot complete and the peer's ACKs cannot arrive, so the RTO
// reports timed_out (same rule as a serve reply with no return link,
// which throws out of run_until - the difference is that TCP's
// machinery never throws from inside the event loop).
//
// EOF is (success, 0 bytes) on a read, not an error code: there is no
// portable eof in std::errc, and inventing a custom category for one
// signal would break the "std errors only" rule kept since v0.6.0.
//
// Non-copyable, movable (a move transfers the shared connection state;
// the network registry holds the state, not the wrapper, so no
// registry update is needed on a move). Must not outlive the network -
// same rule as node references.

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <functional>
#include <map>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

#include "simulnetx/message.hxx"
#include "simulnetx/network.hxx"
#include "simulnetx/node.hxx"

namespace simulnetx::tcp {

// An address in the sim: a node plus a port. Node ids are addresses;
// there is no IP layer underneath. TCP and UDP ports are independent
// namespaces: TCP 8080 and UDP 8080 on one node are different addresses.
struct endpoint {
    node_id node{};
    std::uint16_t port{};

    friend bool operator==(const endpoint&, const endpoint&) = default;
};

// "node#3:8080". Names live in the network, so only ids print here.
std::string to_string(const endpoint& ep);

class socket;
class acceptor;

namespace detail {

// Wire flags for the TCP envelope (see serializer.hxx).
inline constexpr std::uint32_t tcp_flag_syn = 1;
inline constexpr std::uint32_t tcp_flag_ack = 2;
inline constexpr std::uint32_t tcp_flag_fin = 4;
inline constexpr std::uint32_t tcp_flag_rst = 8;

// MSS: the largest data payload in one segment (sim-scoped constant).
inline constexpr std::size_t tcp_mss = 1024;

// RTO: first retransmit timeout, doubling on every retransmit.
inline constexpr std::chrono::nanoseconds tcp_rto_initial{200000000};

// Max retransmits of one segment before the connection is torn down.
inline constexpr int tcp_max_retransmits = 5;

// FIN_WAIT_2 timeout: a socket in FIN_WAIT_2 whose peer never sends FIN
// is unregistered after this long (sim-scoped, documented as arbitrary;
// 60s is long enough that no realistic test trips on it accidentally,
// and short enough that a half-open connection is not a permanent
// leak). Firing is a silent timer, so it cannot throw out of run_until.
inline constexpr std::chrono::nanoseconds tcp_fin_wait_2_timeout{60000000000};

// One unacknowledged segment, kept for retransmission. The ACK field is
// NOT stored: a retransmit re-acks with the current receive position.
// flags holds the SYN/FIN bits as sent; the ACK bit is set on every
// retransmit of a data or FIN segment (a bare SYN retransmits bare,
// unless simultaneous open turned it into a SYN+ACK).
struct tcp_pending_segment {
    std::uint64_t seq = 0;
    std::uint32_t flags = 0;
    std::vector<std::byte> payload;
};

// One outstanding write: completes when the cumulative ACK reaches
// end_seq (exclusive).
struct tcp_write_op {
    std::uint64_t end_seq = 0;
    std::size_t total = 0;
    std::function<void(std::error_code, std::size_t)> handler;
};

// Shared mutable state of one TCP connection, owned jointly by the
// socket wrapper the caller holds, the acceptor that owns a server-side
// socket, and the network's connection registry. RTO events hold only a
// weak_ptr to it (like the request engine's retry callbacks), so a
// destroyed socket's timers no-op instead of dereferencing a dead
// object. The socket wrapper is a thin handle over this state.
struct tcp_connection : std::enable_shared_from_this<tcp_connection> {
    enum class state {
        closed,      // bound or unbound, no connection
        syn_sent,    // active open: SYN sent, awaiting SYN+ACK
        syn_rcvd,    // passive open: SYN received, SYN+ACK sent
        established,
        fin_wait_1,  // our FIN sent, awaiting its ACK
        fin_wait_2,  // our FIN acked, awaiting the peer's FIN (a 60s
                      // silent timer frees the 4-tuple if it never comes)
        close_wait,  // peer's FIN received, we have not closed yet
        last_ack,    // our FIN sent after the peer's FIN, awaiting its ACK
        closing,     // simultaneous close: both FINs sent, awaiting ACK
    };

    tcp_connection(network* net, node* owner) : net_(net), node_(owner) {}

    // Socket-facing API (delegated by tcp::socket).
    void bind(std::uint16_t port);
    void ensure_bound();
    endpoint local_endpoint() const;
    endpoint remote_endpoint() const;
    bool is_open() const { return open_; }
    using connect_handler = std::function<void(std::error_code)>;
    void async_connect(endpoint peer, connect_handler handler);
    using write_handler = std::function<void(std::error_code, std::size_t)>;
    void async_write(std::span<const std::byte> data, write_handler handler);
    using read_handler = std::function<void(std::error_code, std::size_t)>;
    void async_read_some(std::span<std::byte> buffer, read_handler handler);
    void close();

    // Network-facing: called by the network's TCP registry on segment
    // delivery, and by network::crash.
    void on_segment(node_id from, std::uint16_t src_port, std::uint32_t flags,
                    std::uint64_t seq, std::uint64_t ack,
                    std::vector<std::byte> payload);
    void on_node_crashed();

    // Acceptor-facing: set up a server-side connection in SYN_RCVD.
    // Registers the connection and arms the RTO; the caller sends the
    // SYN+ACK afterwards.
    void init_server_side(std::uint16_t listen_port, endpoint peer,
                          std::uint64_t peer_seq, acceptor* owner);
    void send_syn_ack();
    // Resends the SYN+ACK for a retransmitted SYN (the first SYN+ACK
    // may have been lost) and restarts the RTO. Never throws.
    void resend_syn_ack();
    // The peer this connection talks to (the acceptor uses it to spot
    // a retransmitted SYN for an existing half-open connection).
    endpoint peer_endpoint() const { return peer_; }
    void set_acceptor(acceptor* owner) { acceptor_ = owner; }
    void detach_acceptor() { acceptor_ = nullptr; }

private:
    friend class socket;
    friend class acceptor;

    void on_rto(std::uint64_t generation);
    // Silent teardown (no sends): for RTO exhaustion in SYN_RCVD. The
    // caller (the acceptor, or the crash path) drops the socket.
    void abandon();
    // RST teardown: complete pending ops with the given code, unregister,
    // release the port claim. The socket is unusable afterwards.
    void reset_with(std::error_code code);
    // Complete every pending op (connect, read, writes) with the code,
    // via zero-delay events.
    void complete_pending(std::error_code code);
    void complete_connect(std::error_code code);
    void complete_writes_up_to(std::uint64_t acked);
    void complete_writes_with(std::error_code code);
    void arm_rto(std::chrono::nanoseconds delay);
    void disarm_rto();
    // FIN_WAIT_2 timeout machinery: arm on entering FIN_WAIT_2, disarm
    // on finish_close; the generation discards stale firings.
    void arm_fin_wait_2_timeout();
    void disarm_fin_wait_2_timeout();
    void on_fin_wait_2_timeout(std::uint64_t generation);
    // Send one segment now (throws on no link / unknown node / crashed
    // sender, like node::send). Only user-initiated sends use this
    // directly; protocol machinery goes through send_best_effort.
    void send_raw(std::uint32_t flags, std::uint64_t seq, std::uint64_t ack,
                  std::vector<std::byte> payload);
    // Best-effort send: failures are swallowed (the RTO degrades to
    // timed_out instead of aborting run_until).
    void send_best_effort(std::uint32_t flags, std::uint64_t seq,
                          std::uint64_t ack, std::vector<std::byte> payload);
    void send_ack();
    // Sends an RST for this connection's 4-tuple. Never throws, and
    // never answers an RST that was already received.
    void send_reset_best_effort();
    void fire_later(std::function<void()> completion);
    void register_connection();
    void unregister_connection();
    void release_port();
    // The FIN handshake is complete: unregister and mark closed. The
    // socket was already closed by the user, so nothing else to do.
    void finish_close();

    // Segment processing (called from on_segment).
    void handle_reset();
    void handle_syn_sent(bool is_syn, bool is_ack, std::uint64_t seq,
                         std::uint64_t ack, std::vector<std::byte> payload);
    void handle_syn_rcvd(bool is_syn, bool is_ack, std::uint64_t seq,
                         std::uint64_t ack, std::vector<std::byte> payload);
    // Absorb payload bytes at seq into the ready buffer (trimming
    // already-received bytes, buffering out-of-order segments).
    // Returns true if recv_next_ advanced.
    bool absorb(std::uint64_t seq, std::vector<std::byte> payload);
    void drain_reassembly();
    // Advance the send window to ack: drop acked segments, complete
    // writes, restart or disarm the RTO, and move the FIN states.
    void advance_window(std::uint64_t ack);
    // Process a FIN whose seq has been reached. Returns false if the
    // connection fully closed (FIN_WAIT_2 + peer FIN -> finish_close).
    bool process_fin();
    // Complete the pending read if bytes are ready or EOF was reached.
    void maybe_complete_read();

    network* net_;
    node* node_;
    acceptor* acceptor_ = nullptr;
    bool open_ = true;
    bool bound_ = false;
    // False for server-side sockets: the acceptor owns the (node, port)
    // claim, so the socket must not release it on close.
    bool owns_port_claim_ = true;
    std::uint16_t port_ = 0;
    bool connected_ = false;
    endpoint peer_{};
    state state_ = state::closed;
    // An RST was already received: close() must not send another one.
    bool reset_received_ = false;
    bool peer_eof_ = false;
    // A FIN that arrived out of order, waiting for its seq to be reached.
    bool fin_queued_ = false;
    std::uint64_t fin_queued_seq_ = 0;

    std::uint64_t send_next_ = 0;   // next seq to send
    std::uint64_t send_acked_ = 0;  // oldest unacked seq (cumulative ACK)
    std::uint64_t recv_next_ = 0;   // next expected seq
    std::uint64_t fin_seq_ = 0;     // seq of our FIN, once sent

    std::deque<tcp_pending_segment> unacked_;
    std::deque<tcp_write_op> writes_;
    std::map<std::uint64_t, std::vector<std::byte>> reassembly_;
    std::deque<std::byte> recv_ready_;

    connect_handler connect_handler_;
    std::span<std::byte> read_buffer_{};
    read_handler read_handler_;
    bool read_pending_ = false;

    std::uint64_t rto_generation_ = 0;
    int rto_retries_ = 0;
    std::chrono::nanoseconds rto_delay_ = tcp_rto_initial;
    bool rto_armed_ = false;
    // Generation for the FIN_WAIT_2 timeout: arm/disarm bump it, so a
    // firing from an earlier arming is stale and ignored.
    std::uint64_t fin_wait_2_generation_ = 0;
};

}  // namespace detail

// A TCP socket bound to one node of one network. Async Asio-style API,
// minimal: no io_context (the network IS the io_context - run_until is
// io_context::run), no strands (the sim is single-threaded by
// construction), no buffer machinery (std::span instead).
//
// A thin wrapper over detail::tcp_connection: all state lives there, so
// a move transfers the connection without touching the network
// registry, and RTO events (which hold only a weak_ptr) can never
// dereference a dead socket.
class socket {
public:
    // Unbound. Both references stay valid for the network's lifetime.
    socket(network& net, node& node);
    ~socket();  // close()

    socket(const socket&) = delete;
    socket& operator=(const socket&) = delete;
    socket(socket&& other) noexcept;
    socket& operator=(socket&& other) noexcept;

    // Bind to a port on this socket's node. Port 0 = ephemeral: the
    // network assigns the lowest free port from 49152 up, per node,
    // from the TCP port namespace (independent of UDP).
    // Throws std::logic_error if already bound, or if the port is
    // already bound on this node. The same port on a DIFFERENT node is
    // fine - the registry is keyed by (node, port).
    void bind(std::uint16_t port);

    // This socket's address. Throws std::logic_error if unbound.
    endpoint local_endpoint() const;

    // The peer's address. Throws std::logic_error if unconnected.
    endpoint remote_endpoint() const;

    // False after close(), after a reset, or after an RTO exhaustion.
    // Stays true across a crash/restart of this socket's node: the
    // binding survives, only the connection dies (see tcp_connection).
    bool is_open() const { return conn_ != nullptr && conn_->is_open(); }

    // Graceful close: FIN after the queued writes (they were all sent
    // already - writes go out immediately), cancels the pending read
    // with operation_canceled. Idempotent. After close() the socket is
    // unusable (async_* throw logic_error) and the port can be rebound
    // immediately - there is no TIME_WAIT.
    void close();

    // Connect to a peer: 3-way handshake. The handler fires with
    // success on ESTABLISHED, connection_refused when the peer has no
    // listener on the port, timed_out when the handshake's RTO
    // exhausts, operation_canceled when the socket is closed or its
    // node crashes first. Sends nothing until called. Throws
    // std::logic_error on an already-connected socket, and
    // synchronously (handler never fires) with no link to the peer, an
    // unknown peer node, or a crashed sender. Connecting from an
    // unbound socket binds an ephemeral port first (Asio rule).
    using connect_handler = std::function<void(std::error_code)>;
    void async_connect(endpoint peer, connect_handler handler);

    // Write bytes to the stream. The data is COPIED, so the span need
    // not outlive the call. Each write completes, in order, when its
    // bytes are all ACKed. A write larger than the MSS is split into
    // 1024-byte segments. A zero-length write is legal and completes
    // immediately with 0 bytes. Throws std::logic_error on a closed or
    // unconnected socket, and synchronously (handler never fires) with
    // no link to the peer or a crashed sender.
    using write_handler = std::function<void(std::error_code, std::size_t)>;
    void async_write(std::span<const std::byte> data, write_handler handler);
    void async_write(std::string_view data, write_handler handler);

    // Read up to buffer.size() bytes of the stream, in order. The
    // buffer is CALLER-OWNED and must outlive the operation (Asio
    // rule). At most ONE pending read per socket; a second
    // async_read_some while one is pending throws std::logic_error.
    // A read completes with (success, 0) at EOF, after the buffered
    // bytes have been drained. A read is legal after close()
    // (half-close), and on a socket whose peer already sent FIN and
    // whose connection fully closed before the read was armed (reports
    // EOF at once). Throws std::logic_error on a never-connected or
    // abnormally-closed socket.
    using read_handler = std::function<void(std::error_code, std::size_t)>;
    void async_read_some(std::span<std::byte> buffer, read_handler handler);

private:
    friend class acceptor;
    friend struct detail::tcp_connection;

    explicit socket(std::shared_ptr<detail::tcp_connection> conn)
        : conn_(std::move(conn)) {}

    // The shared state, for the acceptor (which hands sockets out) and
    // for the connection itself. Null for a moved-from socket.
    std::shared_ptr<detail::tcp_connection> connection() const { return conn_; }

    // Detach the state without closing it: for the acceptor, which
    // hands a socket to the accept handler (the destructor must not
    // close a connection the user now owns).
    std::shared_ptr<detail::tcp_connection> release() {
        auto taken = std::move(conn_);
        conn_ = nullptr;
        return taken;
    }

    // The target of a delegating call. Throws std::logic_error on a
    // moved-from socket.
    detail::tcp_connection& target() {
        if (conn_ == nullptr) {
            throw std::logic_error("simulnetx: operation on a moved-from socket");
        }
        return *conn_;
    }
    const detail::tcp_connection& target() const {
        if (conn_ == nullptr) {
            throw std::logic_error("simulnetx: operation on a moved-from socket");
        }
        return *conn_;
    }

    std::shared_ptr<detail::tcp_connection> conn_;
};

// A TCP listener: binds a port, accepts incoming connections. The
// backlog bounds the queue of completed handshakes waiting for accept;
// a SYN arriving while it is full is dropped silently, and the
// client's RTO reports timed_out. One pending accept at a time, like
// the one-pending-read rule on sockets.
class acceptor {
public:
    acceptor(network& net, node& node);
    ~acceptor();  // close()

    acceptor(const acceptor&) = delete;
    acceptor& operator=(const acceptor&) = delete;
    acceptor(acceptor&& other) noexcept;
    acceptor& operator=(acceptor&& other) noexcept;

    // Bind to a port on this acceptor's node. Port 0 = ephemeral.
    // Throws std::logic_error if already bound, or if the port is
    // already bound on this node (by a socket or another acceptor).
    void bind(std::uint16_t port);

    // Start listening with a backlog of completed handshakes (default
    // 8). Throws std::logic_error if unbound or already listening.
    void listen(std::size_t backlog = 8);

    // This acceptor's address. Throws std::logic_error if unbound.
    endpoint local_endpoint() const;

    // False after close(). close() is idempotent: it unregisters the
    // listener, drops queued connections, and cancels the pending
    // accept (if any) with operation_canceled. The port can be rebound
    // immediately afterwards.
    bool is_open() const { return open_; }
    void close();

    // Accept one connection: the handler receives the connected socket
    // (moved out of the acceptor) and a clear error_code, or a closed
    // socket with operation_canceled when the acceptor is closed or its
    // node crashes. At most ONE pending accept; a second async_accept
    // while one is pending throws std::logic_error. If a completed
    // handshake is already queued, the handler fires via a zero-delay
    // event - never synchronously.
    using accept_handler = std::function<void(socket, std::error_code)>;
    void async_accept(accept_handler handler);

private:
    // Qualified: an unqualified `friend class network;` here would
    // declare a NEW class simulnetx::tcp::network (friend lookup for an
    // elaborated-type-specifier considers only the innermost enclosing
    // namespace) instead of befriending simulnetx::network.
    friend class ::simulnetx::network;
    friend struct detail::tcp_connection;

    // Called by the network when a bare SYN arrives for this
    // acceptor's port. Creates the server-side connection in SYN_RCVD,
    // sends the SYN+ACK, and queues it as half-open.
    void on_syn(node_id from, std::uint16_t src_port, std::uint64_t seq);

    // Called by the network when this acceptor's node crashes: queued
    // connections are dropped and the pending accept is cancelled. The
    // listener binding survives the restart.
    void on_node_crashed();

    // A half-open connection completed its handshake: move it to the
    // completed queue, or hand it to the pending accept. Called by the
    // connection's on_segment.
    void complete_handshake(std::shared_ptr<detail::tcp_connection> conn);

    // Drop a half-open connection (RST received, RTO exhaustion, or
    // crash): erase its socket; the destructor's close() is a no-op
    // because the connection was already torn down.
    void drop_half_open(std::shared_ptr<detail::tcp_connection> conn);

    // Move-transfer helper, shared by the move constructor and move
    // assignment: takes other's state, repoints the listener registry
    // entry and the connections' back-pointers, and leaves other
    // closed with no pending accept.
    void adopt(acceptor&& other) noexcept;

    // Complete an accept via a zero-delay event (never synchronously).
    // The socket is built from the connection at fire time, so the
    // deferred lambda only holds copyable state.
    void fire_accept_later(accept_handler handler,
                           std::shared_ptr<detail::tcp_connection> conn,
                           std::error_code ec);

    network* net_;
    node* node_;
    bool open_ = true;
    bool bound_ = false;
    bool listening_ = false;
    std::uint16_t port_ = 0;
    std::size_t backlog_ = 0;
    std::deque<socket> completed_;
    std::deque<socket> half_open_;
    accept_handler accept_handler_;
    bool accept_pending_ = false;
};

}  // namespace simulnetx::tcp
