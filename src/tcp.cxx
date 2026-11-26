#include "simulnetx/tcp.hxx"

#include <algorithm>
#include <cstring>
#include <stdexcept>
#include <utility>

namespace simulnetx::tcp {

std::string to_string(const endpoint& ep) {
    return "node#" + std::to_string(ep.node) + ":" + std::to_string(ep.port);
}

namespace detail {



void tcp_connection::bind(std::uint16_t port) {
    if (!open_) {
        throw std::logic_error("simulnetx: bind on a closed socket");
    }
    if (bound_) {
        throw std::logic_error("simulnetx: socket is already bound");
    }
    port_ = net_->claim_tcp_port(node_->id(), port);
    bound_ = true;
}

void tcp_connection::ensure_bound() {
    if (bound_) {
        return;
    }
    port_ = net_->claim_tcp_port(node_->id(), 0);
    bound_ = true;
}

endpoint tcp_connection::local_endpoint() const {
    if (!bound_) {
        throw std::logic_error("simulnetx: local_endpoint on an unbound socket");
    }
    return endpoint{node_->id(), port_};
}

endpoint tcp_connection::remote_endpoint() const {
    if (!open_ || !connected_) {
        throw std::logic_error("simulnetx: remote_endpoint on an unconnected socket");
    }
    return peer_;
}

void tcp_connection::init_server_side(std::uint16_t listen_port, endpoint peer,
                                      std::uint64_t peer_seq, acceptor* owner) {
    bound_ = true;
    owns_port_claim_ = false;  // the acceptor owns the (node, port) claim
    port_ = listen_port;
    peer_ = peer;
    acceptor_ = owner;
    recv_next_ = peer_seq + 1;
    send_next_ = 1;  // our SYN (seq 0) consumes 1
    state_ = state::syn_rcvd;
    unacked_.push_back(tcp_pending_segment{0, tcp_flag_syn | tcp_flag_ack, {}});
    register_connection();
}

void tcp_connection::send_syn_ack() {
    send_raw(tcp_flag_syn | tcp_flag_ack, 0, recv_next_, {});
}

void tcp_connection::handle_reset() {
    if (state_ == state::syn_sent) {
        // Refused: either no listener on the peer's port (the
        // network-generated RST) or the peer went away mid-handshake.
        reset_with(std::make_error_code(std::errc::connection_refused));
        return;
    }
    if (state_ == state::syn_rcvd) {
        // The peer went away mid-handshake: drop silently. The
        // acceptor erases this socket; the destructor's close() is a
        // no-op because reset_with already tore it down. A client
        // socket that reached SYN_RCVD via simultaneous open has a
        // pending connect handler, and it must report connection_reset
        // (never success); a server half-open socket has none.
        reset_received_ = true;
        reset_with(connect_handler_
                       ? std::make_error_code(std::errc::connection_reset)
                       : std::error_code{});
        if (acceptor_ != nullptr) {
            acceptor* owner = acceptor_;
            acceptor_ = nullptr;
            owner->drop_half_open(shared_from_this());
        }
        return;
    }
    reset_with(std::make_error_code(std::errc::connection_reset));
}

void tcp_connection::complete_pending(std::error_code code) {
    complete_connect(code);
    complete_writes_with(code);
    if (read_pending_) {
        read_pending_ = false;
        read_handler handler = std::move(read_handler_);
        read_handler_ = nullptr;
        read_buffer_ = {};
        fire_later([handler = std::move(handler), code] { handler(code, 0); });
    }
}

void tcp_connection::complete_connect(std::error_code code) {
    if (!connect_handler_) {
        return;
    }
    connect_handler handler = std::move(connect_handler_);
    connect_handler_ = nullptr;
    fire_later([handler = std::move(handler), code] { handler(code); });
}

void tcp_connection::complete_writes_with(std::error_code code) {
    while (!writes_.empty()) {
        tcp_write_op op = std::move(writes_.front());
        writes_.pop_front();
        fire_later([handler = std::move(op.handler), code] { handler(code, 0); });
    }
}

void tcp_connection::send_raw(std::uint32_t flags, std::uint64_t seq,
                              std::uint64_t ack, std::vector<std::byte> payload) {
    node_->send_segment(peer_.node, port_, peer_.port, flags, seq, ack,
                        std::move(payload));
}

void tcp_connection::send_best_effort(std::uint32_t flags, std::uint64_t seq,
                                      std::uint64_t ack,
                                      std::vector<std::byte> payload) {
    try {
        send_raw(flags, seq, ack, std::move(payload));
    } catch (...) {
        // Protocol machinery never aborts run_until: the RTO degrades
        // to timed_out instead.
    }
}

void tcp_connection::send_ack() {
    send_best_effort(tcp_flag_ack, send_next_, recv_next_, {});
}

void tcp_connection::send_reset_best_effort() {
    if (reset_received_) {
        return;  // An RST never answers an RST.
    }
    try {
        node_->send_segment(peer_.node, port_, peer_.port, tcp_flag_rst, 0, 0, {});
    } catch (...) {
    }
}

void tcp_connection::fire_later(std::function<void()> completion) {
    net_->schedule_silent(std::move(completion));
}

void tcp_connection::register_connection() {
    net_->register_tcp_connection(
        network::tcp_key{node_->id(), port_, peer_.node, peer_.port},
        shared_from_this());
}

void tcp_connection::unregister_connection() {
    net_->unregister_tcp_connection(
        network::tcp_key{node_->id(), port_, peer_.node, peer_.port});
}

void tcp_connection::release_port() {
    if (!bound_) {
        return;
    }
    if (owns_port_claim_) {
        net_->release_tcp_port(node_->id(), port_);
    }
    bound_ = false;
}

void tcp_connection::close() {
    // Simple teardown (v0.7.0 commit 1): no FIN yet - the graceful FIN
    // close lands with the retransmit/close commit. The socket just
    // goes away; the peer learns via its own RTO.
    if (!open_) {
        return;
    }
    if (state_ == state::syn_sent) {
        send_reset_best_effort();
        complete_connect(std::make_error_code(std::errc::operation_canceled));
    }
    if (state_ == state::syn_rcvd) {
        send_reset_best_effort();
    }
    unregister_connection();
    release_port();
    open_ = false;
    connected_ = false;
    state_ = state::closed;
}

void tcp_connection::on_segment(node_id from, std::uint16_t src_port,
                                std::uint32_t flags, std::uint64_t seq,
                                std::uint64_t ack, std::vector<std::byte> payload) {
    // Handshake-only dispatch (v0.7.0 commit 1): the stream (data/ACK/
    // FIN in ESTABLISHED) lands with the write/read commit. Until then
    // a segment for an established connection is dropped.
    if (from != peer_.node || src_port != peer_.port) {
        return;
    }
    const bool is_rst = (flags & tcp_flag_rst) != 0;
    if (is_rst) {
        handle_reset();
        return;
    }
    const bool is_syn = (flags & tcp_flag_syn) != 0;
    const bool is_ack = (flags & tcp_flag_ack) != 0;
    if (state_ == state::closed) {
        return;
    }
    if (state_ == state::syn_sent) {
        handle_syn_sent(is_syn, is_ack, seq, ack, std::move(payload));
        return;
    }
    if (state_ == state::syn_rcvd) {
        handle_syn_rcvd(is_syn, is_ack, seq, ack, std::move(payload));
        return;
    }
    // Established and closing states: no stream yet, drop the segment.
}

void tcp_connection::on_node_crashed() {
    // Minimal teardown (v0.7.0 commit 1): the full version (clearing
    // reassembly/recv buffers, disarming the RTO) lands with the
    // retransmit/close/crash commit.
    if (!open_) {
        return;
    }
    complete_pending(std::make_error_code(std::errc::operation_canceled));
    unregister_connection();
    connected_ = false;
    peer_eof_ = false;
    fin_queued_ = false;
    state_ = state::closed;
    if (acceptor_ != nullptr) {
        acceptor* owner = acceptor_;
        acceptor_ = nullptr;
        owner->drop_half_open(shared_from_this());
    }
}

void tcp_connection::resend_syn_ack() {
    // No RTO restart yet (v0.7.0 commit 1): the RTO lands with the
    // retransmit commit, which also restarts it here.
    send_best_effort(tcp_flag_syn | tcp_flag_ack, 0, recv_next_, {});
}

void tcp_connection::handle_syn_sent(bool is_syn, bool is_ack, std::uint64_t seq,
                                     std::uint64_t ack,
                                     std::vector<std::byte>) {
    if (is_syn && is_ack) {
        if (ack != send_next_) {
            return;  // Not our SYN being acked: stray.
        }
        recv_next_ = seq + 1;
        if (!unacked_.empty() && unacked_.front().seq == 0) {
            unacked_.pop_front();
        }
        state_ = state::established;
        connected_ = true;
        send_ack();
        complete_connect(std::error_code{});
        return;
    }
    if (is_syn) {
        // Simultaneous open: the peer also sent a SYN. Answer with
        // SYN+ACK and wait for their ACK like a server would.
        recv_next_ = seq + 1;
        if (!unacked_.empty() && unacked_.front().seq == 0) {
            unacked_.front().flags = tcp_flag_syn | tcp_flag_ack;
        }
        send_best_effort(tcp_flag_syn | tcp_flag_ack, 0, recv_next_, {});
        state_ = state::syn_rcvd;
        return;
    }
    // A bare ACK or anything else in SYN_SENT is stray: drop it.
}

void tcp_connection::handle_syn_rcvd(bool is_syn, bool is_ack, std::uint64_t,
                                     std::uint64_t ack,
                                     std::vector<std::byte>) {
    if (is_ack && !is_syn) {
        if (ack != send_next_) {
            return;  // Not our SYN being acked: stray.
        }
        if (!unacked_.empty() && unacked_.front().seq == 0) {
            unacked_.pop_front();
        }
        state_ = state::established;
        connected_ = true;
        if (acceptor_ != nullptr) {
            acceptor_->complete_handshake(shared_from_this());
        } else {
            complete_connect(std::error_code{});
        }
        return;
    }
    if (is_syn && !is_ack) {
        // Retransmitted SYN: resend our SYN+ACK.
        send_best_effort(tcp_flag_syn | tcp_flag_ack, 0, recv_next_, {});
        return;
    }
    // Anything else: drop.
}

void tcp_connection::reset_with(std::error_code code) {
    // No disarm_rto yet (v0.7.0 commit 1): the RTO lands with the
    // retransmit commit.
    complete_pending(code);
    unregister_connection();
    release_port();
    open_ = false;
    connected_ = false;
    state_ = state::closed;
}

}  // namespace detail


socket::socket(network& net, node& node)
    : conn_(std::make_shared<detail::tcp_connection>(&net, &node)) {}

socket::~socket() { close(); }

socket::socket(socket&& other) noexcept : conn_(std::move(other.conn_)) {}

socket& socket::operator=(socket&& other) noexcept {
    if (this != &other) {
        close();
        conn_ = std::move(other.conn_);
    }
    return *this;
}

void socket::bind(std::uint16_t port) { target().bind(port); }

endpoint socket::local_endpoint() const { return target().local_endpoint(); }

endpoint socket::remote_endpoint() const { return target().remote_endpoint(); }

void socket::close() {
    if (conn_) {
        conn_->close();
    }
}

acceptor::acceptor(network& net, node& node) : net_(&net), node_(&node) {}

acceptor::~acceptor() { close(); }

acceptor::acceptor(acceptor&& other) noexcept : net_(other.net_), node_(other.node_) {
    adopt(std::move(other));
}

acceptor& acceptor::operator=(acceptor&& other) noexcept {
    if (this != &other) {
        close();
        net_ = other.net_;
        node_ = other.node_;
        adopt(std::move(other));
    }
    return *this;
}

void acceptor::adopt(acceptor&& other) noexcept {
    open_ = other.open_;
    bound_ = other.bound_;
    listening_ = other.listening_;
    port_ = other.port_;
    backlog_ = other.backlog_;
    completed_ = std::move(other.completed_);
    half_open_ = std::move(other.half_open_);
    accept_handler_ = std::move(other.accept_handler_);
    accept_pending_ = other.accept_pending_;
    if (listening_) {
        net_->unregister_tcp_listener(node_->id(), port_);
        net_->register_tcp_listener(node_->id(), port_, this);
    }
    // Repoint the connections' back-pointers at this acceptor.
    for (auto& sock : completed_) {
        if (auto conn = sock.connection()) {
            conn->set_acceptor(this);
        }
    }
    for (auto& sock : half_open_) {
        if (auto conn = sock.connection()) {
            conn->set_acceptor(this);
        }
    }
    other.open_ = false;
    other.bound_ = false;
    other.listening_ = false;
    other.accept_pending_ = false;
    other.accept_handler_ = nullptr;
}

void acceptor::bind(std::uint16_t port) {
    if (!open_) {
        throw std::logic_error("simulnetx: bind on a closed acceptor");
    }
    if (bound_) {
        throw std::logic_error("simulnetx: acceptor is already bound");
    }
    port_ = net_->claim_tcp_port(node_->id(), port);
    bound_ = true;
}

void acceptor::listen(std::size_t backlog) {
    if (!open_) {
        throw std::logic_error("simulnetx: listen on a closed acceptor");
    }
    if (!bound_) {
        throw std::logic_error("simulnetx: listen on an unbound acceptor");
    }
    if (listening_) {
        throw std::logic_error("simulnetx: acceptor is already listening");
    }
    backlog_ = backlog;
    net_->register_tcp_listener(node_->id(), port_, this);
    listening_ = true;
}

endpoint acceptor::local_endpoint() const {
    if (!bound_) {
        throw std::logic_error("simulnetx: local_endpoint on an unbound acceptor");
    }
    return endpoint{node_->id(), port_};
}

void acceptor::close() {
    if (!open_) {
        return;
    }
    open_ = false;
    listening_ = false;
    if (bound_) {
        net_->unregister_tcp_listener(node_->id(), port_);
        net_->release_tcp_port(node_->id(), port_);
        bound_ = false;
    }
    // Detach the back-pointers first: the connections are about to be
    // destroyed, and their RTO events (weak_ptr) must never call back
    // into this acceptor.
    for (auto& sock : completed_) {
        if (auto conn = sock.connection()) {
            conn->detach_acceptor();
        }
    }
    for (auto& sock : half_open_) {
        if (auto conn = sock.connection()) {
            conn->detach_acceptor();
        }
    }
    completed_.clear();
    half_open_.clear();
    if (accept_pending_) {
        accept_pending_ = false;
        accept_handler handler = std::move(accept_handler_);
        accept_handler_ = nullptr;
        auto dead = std::make_shared<detail::tcp_connection>(net_, node_);
        dead->close();
        fire_accept_later(std::move(handler), std::move(dead),
                          std::make_error_code(std::errc::operation_canceled));
    }
}

void acceptor::on_syn(node_id from, std::uint16_t src_port, std::uint64_t seq) {
    if (!open_ || !listening_) {
        return;  // Defensive: the registry unregisters on close.
    }
    if (completed_.size() >= backlog_) {
        return;  // Backlog full: drop the SYN silently (Part 5).
    }
    // A retransmitted SYN for an existing half-open connection: resend
    // the SYN+ACK (the first one may have been lost) and restart the
    // RTO. Creating a second connection would collide on the 4-tuple.
    for (auto& sock : half_open_) {
        if (auto conn = sock.connection()) {
            if (conn->peer_endpoint() == endpoint{from, src_port}) {
                conn->resend_syn_ack();
                return;
            }
        }
    }
    auto conn = std::make_shared<detail::tcp_connection>(net_, node_);
    conn->init_server_side(port_, endpoint{from, src_port}, seq, this);
    // The SYN+ACK is best-effort: a failure degrades to RTO timeouts,
    // never an exception out of run_until.
    try {
        conn->send_syn_ack();
    } catch (...) {
    }
    half_open_.emplace_back(socket(conn));
}

void acceptor::on_node_crashed() {
    // The queued connections were already torn down by the network's
    // connection notifications (their bindings survive, but the
    // sockets are dead); drop the wrappers. The listener binding
    // survives, so the acceptor works again after the restart.
    completed_.clear();
    half_open_.clear();
    if (accept_pending_) {
        accept_pending_ = false;
        accept_handler handler = std::move(accept_handler_);
        accept_handler_ = nullptr;
        auto dead = std::make_shared<detail::tcp_connection>(net_, node_);
        dead->close();
        fire_accept_later(std::move(handler), std::move(dead),
                          std::make_error_code(std::errc::operation_canceled));
    }
}

void acceptor::complete_handshake(std::shared_ptr<detail::tcp_connection> conn) {
    for (auto it = half_open_.begin(); it != half_open_.end(); ++it) {
        if (it->connection() == conn) {
            socket taken = std::move(*it);
            half_open_.erase(it);
            if (accept_pending_) {
                accept_pending_ = false;
                accept_handler handler = std::move(accept_handler_);
                accept_handler_ = nullptr;
                conn->detach_acceptor();
                fire_accept_later(std::move(handler), taken.release(),
                                  std::error_code{});
            } else if (completed_.size() < backlog_) {
                completed_.push_back(std::move(taken));
            } else {
                // The completed queue filled between the SYN and the
                // ACK: the connection is closed with a FIN (the client
                // sees EOF). Dropping it silently would leave the
                // client hanging until its RTO.
                // (taken is destroyed here; its close() sends the FIN.)
            }
            return;
        }
    }
}

void acceptor::drop_half_open(std::shared_ptr<detail::tcp_connection> conn) {
    for (auto it = half_open_.begin(); it != half_open_.end(); ++it) {
        if (it->connection() == conn) {
            half_open_.erase(it);
            return;
        }
    }
}

void acceptor::fire_accept_later(accept_handler handler,
                                 std::shared_ptr<detail::tcp_connection> conn,
                                 std::error_code ec) {
    net_->schedule_silent([handler = std::move(handler), conn = std::move(conn), ec] {
        handler(socket(conn), ec);
    });
}

}  // namespace simulnetx::tcp
