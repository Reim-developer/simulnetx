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

namespace {

// The last sequence number a segment covers: its payload bytes, plus
// one for a SYN or FIN flag (each consumes one sequence number).
std::uint64_t segment_end(const tcp_pending_segment& seg) {
    std::uint64_t end = seg.seq + seg.payload.size();
    if ((seg.flags & (tcp_flag_syn | tcp_flag_fin)) != 0) {
        end += 1;
    }
    return end;
}

}  // namespace

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

void tcp_connection::async_connect(endpoint peer, connect_handler handler) {
    if (!open_) {
        throw std::logic_error("simulnetx: async_connect on a closed socket");
    }
    if (state_ != state::closed) {
        throw std::logic_error("simulnetx: async_connect on a connected socket");
    }
    ensure_bound();
    connect_handler_ = std::move(handler);
    peer_ = peer;
    // Send the SYN first: nothing is registered yet, so a synchronous
    // send failure (no link, unknown node, crashed sender) leaves no
    // state behind and the handler never fires.
    send_raw(tcp_flag_syn, 0, 0, {});
    send_next_ = 1;
    unacked_.push_back(tcp_pending_segment{0, tcp_flag_syn, {}});
    state_ = state::syn_sent;
    try {
        register_connection();
    } catch (...) {
        // Unreachable in practice (the ephemeral port is fresh, so no
        // 4-tuple collision is possible), but leave no half-built
        // state behind if it ever happens.
        state_ = state::closed;
        unacked_.clear();
        connect_handler_ = nullptr;
        throw;
    }
    arm_rto(tcp_rto_initial);
}

void tcp_connection::async_write(std::span<const std::byte> data,
                                 write_handler handler) {
    if (!open_) {
        throw std::logic_error("simulnetx: async_write on a closed socket");
    }
    if (state_ != state::established && state_ != state::close_wait) {
        throw std::logic_error("simulnetx: async_write on an unconnected socket");
    }
    if (data.empty()) {
        // Legal, sends nothing: complete via a zero-delay event, never
        // synchronously.
        fire_later([handler = std::move(handler)] { handler(std::error_code{}, 0); });
        return;
    }
    // Queue the write before sending: the segments are numbered from
    // send_next_ in order, so the op's end_seq is known up front. If a
    // send throws (unreachable in practice: every segment takes the
    // same link), the op stays queued and its bytes complete on ACK or
    // on teardown, so no write is silently lost.
    tcp_write_op op;
    op.end_seq = send_next_ + data.size();
    op.total = data.size();
    op.handler = std::move(handler);
    writes_.push_back(std::move(op));
    std::size_t offset = 0;
    while (offset < data.size()) {
        const std::size_t chunk = std::min(tcp_mss, data.size() - offset);
        const auto begin = data.begin() + static_cast<std::ptrdiff_t>(offset);
        std::vector<std::byte> payload(begin, begin + static_cast<std::ptrdiff_t>(chunk));
        const std::uint64_t seq = send_next_;
        send_raw(tcp_flag_ack, seq, recv_next_, payload);
        unacked_.push_back(tcp_pending_segment{seq, tcp_flag_ack, std::move(payload)});
        send_next_ += chunk;
        offset += chunk;
    }
    if (!rto_armed_) {
        arm_rto(tcp_rto_initial);
    }
}

void tcp_connection::async_read_some(std::span<std::byte> buffer,
                                     read_handler handler) {
    // The peer already sent its FIN and the connection fully closed
    // before this read was armed (the handler that arms it runs via a
    // deferred event, so the FIN can land first): report EOF at once.
    if (state_ == state::closed && peer_eof_) {
        fire_later([handler = std::move(handler)] { handler(std::error_code{}, 0); });
        return;
    }
    // No open_ check here: after close() the socket is still readable
    // (half-close: FIN_WAIT_1/2/CLOSING). The state check below rejects
    // reads on a never-connected or fully-closed socket.
    if (state_ != state::established && state_ != state::close_wait &&
        state_ != state::fin_wait_1 && state_ != state::fin_wait_2 &&
        state_ != state::closing) {
        throw std::logic_error("simulnetx: async_read_some on an unconnected socket");
    }
    if (read_pending_) {
        throw std::logic_error("simulnetx: a read is already pending on this socket");
    }
    read_buffer_ = buffer;
    read_handler_ = std::move(handler);
    read_pending_ = true;
    maybe_complete_read();
}

void tcp_connection::close() {
    if (!open_) {
        return;
    }
    if (state_ == state::closed) {
        // No connection to tear down (never connected, or already dead
        // via crash/reset/timeout): just release the port claim.
        release_port();
        open_ = false;
        return;
    }
    if (state_ == state::syn_sent || state_ == state::syn_rcvd) {
        // Abandon the handshake: RST the peer (best-effort) and cancel
        // the connect handler if it is ours.
        send_reset_best_effort();
        if (state_ == state::syn_sent) {
            complete_connect(std::make_error_code(std::errc::operation_canceled));
        }
        unregister_connection();
        disarm_rto();
        release_port();
        open_ = false;
        connected_ = false;
        state_ = state::closed;
        return;
    }
    if (state_ == state::established || state_ == state::close_wait) {
        // Graceful close: FIN after the queued writes (they were all
        // sent already - writes go out immediately, so there is nothing
        // to drain). A pending read is cancelled; pending writes stay
        // queued and complete on ACK (the FIN's ACK implies them).
        const std::uint64_t fin_seq = send_next_;
        fin_seq_ = fin_seq;
        send_best_effort(tcp_flag_fin | tcp_flag_ack, fin_seq, recv_next_, {});
        unacked_.push_back(
            tcp_pending_segment{fin_seq, tcp_flag_fin | tcp_flag_ack, {}});
        send_next_ += 1;
        state_ = state_ == state::established ? state::fin_wait_1 : state::last_ack;
        if (read_pending_) {
            read_pending_ = false;
            read_handler handler = std::move(read_handler_);
            read_handler_ = nullptr;
            read_buffer_ = {};
            fire_later([handler = std::move(handler)] {
                handler(std::make_error_code(std::errc::operation_canceled), 0);
            });
        }
        if (!rto_armed_) {
            arm_rto(tcp_rto_initial);
        }
        release_port();
        open_ = false;
        connected_ = false;
        return;
    }
    // Already closing (fin_wait_1/2, last_ack, closing): close() is
    // idempotent for the state machine, and open_ is already false,
    // so this path is unreachable. Defensive no-op.
}

void tcp_connection::on_segment(node_id from, std::uint16_t src_port,
                                std::uint32_t flags, std::uint64_t seq,
                                std::uint64_t ack, std::vector<std::byte> payload) {
    // The 4-tuple registry guarantees this segment is for us; the
    // check below just documents the invariant (a mismatch is dropped,
    // never misrouted).
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
    const bool is_fin = (flags & tcp_flag_fin) != 0;

    if (state_ == state::closed) {
        return;  // Not registered in this state; defensive only.
    }
    if (state_ == state::syn_sent) {
        handle_syn_sent(is_syn, is_ack, seq, ack, std::move(payload));
        return;
    }
    if (state_ == state::syn_rcvd) {
        handle_syn_rcvd(is_syn, is_ack, seq, ack, std::move(payload));
        return;
    }
    // Established and closing states: data, ACK, FIN.
    bool ack_needed = false;
    // The FIN's seq is the seq after the data in this segment.
    const std::uint64_t fin_seq = seq + payload.size();
    if (!payload.empty()) {
        if (absorb(seq, std::move(payload))) {
            ack_needed = true;
            if (fin_queued_ && fin_queued_seq_ == recv_next_) {
                fin_queued_ = false;
                if (!process_fin()) {
                    return;  // Fully closed (FIN_WAIT_2 + peer FIN).
                }
                ack_needed = true;
            }
        }
    }
    if (is_ack) {
        advance_window(ack);
        if (state_ == state::closed) {
            return;  // finish_close() unregistered us (LAST_ACK/CLOSING).
        }
    }
    if (is_fin) {
        if (fin_seq == recv_next_) {
            if (!process_fin()) {
                return;  // Fully closed (FIN_WAIT_2 + peer FIN).
            }
            ack_needed = true;
        } else if (fin_seq > recv_next_) {
            // Out of order: wait for the missing bytes. No ACK yet -
            // the peer retransmits the FIN on its RTO.
            fin_queued_ = true;
            fin_queued_seq_ = fin_seq;
        } else {
            // Retransmitted FIN: re-ACK it.
            ack_needed = true;
        }
    }
    if (is_syn) {
        // A retransmitted SYN+ACK (our ACK was lost): re-ACK it so the
        // peer can complete the handshake. Without this the peer's
        // half-open connection would retransmit until it times out,
        // even though we are already established.
        ack_needed = true;
    }
    if (ack_needed && state_ != state::closed) {
        send_best_effort(tcp_flag_ack, send_next_, recv_next_, {});
    }
    maybe_complete_read();
}

void tcp_connection::on_node_crashed() {
    if (!open_) {
        return;  // Already closed: nothing to cancel.
    }
    // Cancel every pending op (connect, read, writes) with
    // operation_canceled; drop all connection state. The binding
    // survives: bound_ and port_ are untouched, so the socket can
    // connect again after the restart.
    complete_pending(std::make_error_code(std::errc::operation_canceled));
    unregister_connection();
    disarm_rto();
    unacked_.clear();
    writes_.clear();
    reassembly_.clear();
    recv_ready_.clear();
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
    arm_rto(tcp_rto_initial);
}

void tcp_connection::send_syn_ack() {
    send_raw(tcp_flag_syn | tcp_flag_ack, 0, recv_next_, {});
}

void tcp_connection::resend_syn_ack() {
    // The SYN+ACK is already the unacked segment (from
    // init_server_side), so this just re-sends it and restarts the RTO.
    send_best_effort(tcp_flag_syn | tcp_flag_ack, 0, recv_next_, {});
    rto_retries_ = 0;
    arm_rto(tcp_rto_initial);
}

void tcp_connection::on_rto(std::uint64_t generation) {
    if (generation != rto_generation_ || !rto_armed_) {
        return;  // Stale timer (disarmed or superseded).
    }
    if (unacked_.empty()) {
        disarm_rto();
        return;
    }
    if (rto_retries_ >= tcp_max_retransmits) {
        // Give up: the connection is torn down and pending ops report
        // timed_out. A server-side half-open connection is dropped
        // silently (the client's RTO reports timed_out too); any other
        // state sends an RST so the peer learns fast.
        if (state_ == state::syn_rcvd) {
            abandon();
            // A client socket that reached SYN_RCVD via simultaneous
            // open has a pending connect handler; a server half-open
            // socket has none (complete_connect is a no-op then).
            complete_connect(std::make_error_code(std::errc::timed_out));
            if (acceptor_ != nullptr) {
                acceptor* owner = acceptor_;
                acceptor_ = nullptr;
                owner->drop_half_open(shared_from_this());
            }
            return;
        }
        send_reset_best_effort();
        complete_pending(std::make_error_code(std::errc::timed_out));
        unregister_connection();
        disarm_rto();
        release_port();
        open_ = false;
        connected_ = false;
        state_ = state::closed;
        return;
    }
    // Retransmit the oldest unacked segment with a fresh ACK.
    const tcp_pending_segment seg = unacked_.front();
    send_best_effort(seg.flags, seg.seq, recv_next_, seg.payload);
    ++rto_retries_;
    rto_delay_ *= 2;
    arm_rto(rto_delay_);
}

void tcp_connection::abandon() {
    unregister_connection();
    disarm_rto();
    open_ = false;
    connected_ = false;
    state_ = state::closed;
}

void tcp_connection::reset_with(std::error_code code) {
    complete_pending(code);
    unregister_connection();
    disarm_rto();
    release_port();
    open_ = false;
    connected_ = false;
    state_ = state::closed;
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

void tcp_connection::complete_writes_up_to(std::uint64_t acked) {
    while (!writes_.empty() && writes_.front().end_seq <= acked) {
        tcp_write_op op = std::move(writes_.front());
        writes_.pop_front();
        fire_later([handler = std::move(op.handler), total = op.total] {
            handler(std::error_code{}, total);
        });
    }
}

void tcp_connection::complete_writes_with(std::error_code code) {
    while (!writes_.empty()) {
        tcp_write_op op = std::move(writes_.front());
        writes_.pop_front();
        fire_later([handler = std::move(op.handler), code] { handler(code, 0); });
    }
}

void tcp_connection::arm_rto(std::chrono::nanoseconds delay) {
    ++rto_generation_;
    const std::uint64_t generation = rto_generation_;
    rto_armed_ = true;
    rto_delay_ = delay;
    std::weak_ptr<tcp_connection> weak = shared_from_this();
    net_->schedule_silent_at(net_->now() + delay, [weak, generation] {
        if (const auto self = weak.lock()) {
            self->on_rto(generation);
        }
    });
}

void tcp_connection::disarm_rto() {
    ++rto_generation_;
    rto_armed_ = false;
}

void tcp_connection::arm_fin_wait_2_timeout() {
    ++fin_wait_2_generation_;
    const std::uint64_t generation = fin_wait_2_generation_;
    std::weak_ptr<tcp_connection> weak = shared_from_this();
    net_->schedule_silent_at(net_->now() + tcp_fin_wait_2_timeout, [weak, generation] {
        if (const auto self = weak.lock()) {
            self->on_fin_wait_2_timeout(generation);
        }
    });
}

void tcp_connection::disarm_fin_wait_2_timeout() {
    ++fin_wait_2_generation_;
}

void tcp_connection::on_fin_wait_2_timeout(std::uint64_t generation) {
    if (generation != fin_wait_2_generation_ || state_ != state::fin_wait_2) {
        return;  // Stale timer (disarmed, or the peer's FIN already closed us).
    }
    finish_close();
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

void tcp_connection::finish_close() {
    disarm_rto();
    disarm_fin_wait_2_timeout();
    unregister_connection();
    state_ = state::closed;
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

void tcp_connection::handle_syn_sent(bool is_syn, bool is_ack, std::uint64_t seq,
                                     std::uint64_t ack,
                                     std::vector<std::byte> payload) {
    if (is_syn && is_ack) {
        if (ack != send_next_) {
            return;  // Not our SYN being acked: stray.
        }
        recv_next_ = seq + 1;
        if (!unacked_.empty() && unacked_.front().seq == 0) {
            unacked_.pop_front();
        }
        disarm_rto();
        state_ = state::established;
        connected_ = true;
        if (!payload.empty()) {
            // Data riding the SYN+ACK (rare, legal): absorb it at the
            // seq after the peer's SYN.
            absorb(seq + 1, std::move(payload));
        }
        send_ack();
        complete_connect(std::error_code{});
        return;
    }
    if (is_syn) {
        // Simultaneous open: the peer also sent a SYN. Answer with
        // SYN+ACK and wait for their ACK like a server would. The
        // unacked SYN entry's flags become SYN+ACK, so a retransmit
        // re-sends the right segment.
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

void tcp_connection::handle_syn_rcvd(bool is_syn, bool is_ack, std::uint64_t seq,
                                     std::uint64_t ack,
                                     std::vector<std::byte> payload) {
    if (is_ack && !is_syn) {
        if (ack != send_next_) {
            return;  // Not our SYN being acked: stray.
        }
        if (!unacked_.empty() && unacked_.front().seq == 0) {
            unacked_.pop_front();
        }
        disarm_rto();
        state_ = state::established;
        connected_ = true;
        if (!payload.empty()) {
            // Data riding the ACK (sent before ESTABLISHED): absorb it.
            absorb(seq, std::move(payload));
            maybe_complete_read();
        }
        if (acceptor_ != nullptr) {
            acceptor_->complete_handshake(shared_from_this());
        } else {
            // Simultaneous open on a client socket: no acceptor owns
            // us, so the connect completes here.
            complete_connect(std::error_code{});
        }
        return;
    }
    if (is_syn && !is_ack) {
        // Retransmitted SYN: resend our SYN+ACK and restart the RTO.
        send_best_effort(tcp_flag_syn | tcp_flag_ack, 0, recv_next_, {});
        rto_retries_ = 0;
        arm_rto(tcp_rto_initial);
        return;
    }
    // Anything else (bare ACK with the wrong number, data, FIN): drop.
}

bool tcp_connection::absorb(std::uint64_t seq, std::vector<std::byte> payload) {
    if (payload.empty()) {
        return false;
    }
    if (seq + payload.size() <= recv_next_) {
        return false;  // Fully duplicate: already received.
    }
    if (seq < recv_next_) {
        // Partial overlap: trim the already-received prefix.
        const std::size_t trim = static_cast<std::size_t>(recv_next_ - seq);
        payload.erase(payload.begin(),
                      payload.begin() + static_cast<std::ptrdiff_t>(trim));
        seq = recv_next_;
    }
    if (seq > recv_next_) {
        // Out of order: buffer for reassembly (no ACK yet - the peer
        // retransmits the missing bytes on its RTO).
        reassembly_[seq] = std::move(payload);
        return false;
    }
    recv_ready_.insert(recv_ready_.end(), payload.begin(), payload.end());
    recv_next_ += payload.size();
    drain_reassembly();
    return true;
}

void tcp_connection::drain_reassembly() {
    while (true) {
        // Find a buffered segment overlapping recv_next_: the one
        // starting at or before it with the smallest seq.
        auto it = reassembly_.lower_bound(recv_next_);
        if (it != reassembly_.begin()) {
            auto prev = std::prev(it);
            if (prev->first + prev->second.size() > recv_next_) {
                it = prev;  // overlaps from below
            } else {
                // Fully duplicate: drop it and keep draining.
                reassembly_.erase(prev);
                continue;
            }
        }
        if (it == reassembly_.end() || it->first > recv_next_) {
            return;
        }
        std::uint64_t seg_seq = it->first;
        std::vector<std::byte> seg = std::move(it->second);
        reassembly_.erase(it);
        if (seg_seq < recv_next_) {
            const std::size_t trim = static_cast<std::size_t>(recv_next_ - seg_seq);
            if (trim >= seg.size()) {
                continue;  // fully duplicate
            }
            seg.erase(seg.begin(), seg.begin() + static_cast<std::ptrdiff_t>(trim));
            seg_seq = recv_next_;
        }
        recv_ready_.insert(recv_ready_.end(), seg.begin(), seg.end());
        recv_next_ += seg.size();
    }
}

void tcp_connection::advance_window(std::uint64_t ack) {
    if (ack <= send_acked_ || ack > send_next_) {
        return;  // Stale, duplicate, or beyond what was sent: ignore.
    }
    send_acked_ = ack;
    while (!unacked_.empty() && segment_end(unacked_.front()) <= ack) {
        unacked_.pop_front();
    }
    complete_writes_up_to(ack);
    // Our FIN is acked when the cumulative ACK passes its seq.
    if (state_ == state::fin_wait_1 && ack > fin_seq_) {
        state_ = state::fin_wait_2;
        arm_fin_wait_2_timeout();
    } else if (state_ == state::last_ack && ack > fin_seq_) {
        finish_close();
        return;
    } else if (state_ == state::closing && ack > fin_seq_) {
        finish_close();
        return;
    }
    if (unacked_.empty()) {
        disarm_rto();
    } else {
        // The RTO restarts whenever an ACK advances the send window.
        rto_retries_ = 0;
        arm_rto(tcp_rto_initial);
    }
}

bool tcp_connection::process_fin() {
    recv_next_ += 1;
    peer_eof_ = true;
    switch (state_) {
        case state::established:
            state_ = state::close_wait;
            return true;
        case state::fin_wait_1:
            state_ = state::closing;
            return true;
        case state::fin_wait_2:
            // ACK the FIN before unregistering: the peer is done.
            send_best_effort(tcp_flag_ack, send_next_, recv_next_, {});
            finish_close();
            return false;
        default:
            // close_wait, last_ack, closing: already seen this FIN.
            return true;
    }
}

void tcp_connection::maybe_complete_read() {
    if (!read_pending_) {
        return;
    }
    if (!recv_ready_.empty()) {
        const std::size_t n = std::min(read_buffer_.size(), recv_ready_.size());
        if (n > 0) {
            std::copy_n(recv_ready_.begin(), n, read_buffer_.begin());
            recv_ready_.erase(recv_ready_.begin(),
                              recv_ready_.begin() + static_cast<std::ptrdiff_t>(n));
        }
        read_pending_ = false;
        read_handler handler = std::move(read_handler_);
        read_handler_ = nullptr;
        read_buffer_ = {};
        fire_later([handler = std::move(handler), n] { handler(std::error_code{}, n); });
        return;
    }
    if (peer_eof_) {
        read_pending_ = false;
        read_handler handler = std::move(read_handler_);
        read_handler_ = nullptr;
        read_buffer_ = {};
        fire_later([handler = std::move(handler)] { handler(std::error_code{}, 0); });
    }
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

void socket::async_connect(endpoint peer, connect_handler handler) {
    target().async_connect(peer, std::move(handler));
}

void socket::async_write(std::span<const std::byte> data, write_handler handler) {
    target().async_write(data, std::move(handler));
}

void socket::async_write(std::string_view data, write_handler handler) {
    std::vector<std::byte> bytes;
    bytes.reserve(data.size());
    for (const char c : data) {
        bytes.push_back(static_cast<std::byte>(c));
    }
    async_write(std::span<const std::byte>(bytes), std::move(handler));
}

void socket::async_read_some(std::span<std::byte> buffer, read_handler handler) {
    target().async_read_some(buffer, std::move(handler));
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

void acceptor::async_accept(accept_handler handler) {
    if (!open_) {
        throw std::logic_error("simulnetx: async_accept on a closed acceptor");
    }
    if (!listening_) {
        throw std::logic_error("simulnetx: async_accept on a non-listening acceptor");
    }
    if (accept_pending_) {
        throw std::logic_error("simulnetx: an accept is already pending on this acceptor");
    }
    if (!completed_.empty()) {
        socket taken = std::move(completed_.front());
        completed_.pop_front();
        auto conn = taken.release();
        conn->detach_acceptor();
        fire_accept_later(std::move(handler), std::move(conn), std::error_code{});
        return;
    }
    accept_handler_ = std::move(handler);
    accept_pending_ = true;
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
