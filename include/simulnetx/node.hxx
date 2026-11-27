#pragma once

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "simulnetx/errors.hxx"
#include "simulnetx/message.hxx"
#include "simulnetx/request.hxx"
#include "simulnetx/serializer.hxx"
#include "simulnetx/trace.hxx"

namespace simulnetx {

class network;

namespace udp {
class socket;
}  // namespace udp

namespace tcp {
class socket;
class acceptor;

namespace detail {
struct tcp_connection;
}  // namespace detail
}  // namespace tcp

// One endpoint in a simulated network. Nodes are owned by network; references
// returned by network::add_node stay valid for the lifetime of the network.
class node {
public:
    node(const node&) = delete;
    node& operator=(const node&) = delete;
    node(node&&) = delete;
    node& operator=(node&&) = delete;

    // The numeric id assigned at creation (0, 1, 2, ...), used as the
    // address in link/send calls.
    node_id id() const { return id_; }
    // The name given to add_node, used in traces and failure messages.
    const std::string& name() const { return name_; }

    // Set the handler invoked for each delivered message. Replaces any
    // previously set handler. A node with no handler silently ignores
    // delivered messages (they still count as delivered).
    void on_message(std::function<void(const message&)> handler);

    // Register a typed handler for messages sent as T. A node may register
    // several typed handlers (on<hello> + on<world>); each message reaches
    // exactly the matching one, by its type tag. Registering again for the
    // same T replaces the previous handler. Dispatch order on delivery:
    // typed handlers (by tag) first, then the on_message catch-all if no
    // typed handler matches. If neither handles the message, strict mode
    // (the default) throws unhandled_message out of run_until; see
    // network::set_strict_tags. The handler receives only the decoded
    // value; use on_message + message::as<T>() when the sender matters.
    template <serializable T>
    void on(std::function<void(const T&)> handler) {
        typed_handlers_[detail::type_tag<T>()] =
            [handler = std::move(handler)](const message& msg) {
                handler(msg.as<T>());
            };
    }

    // Send a message to another node. Throws std::invalid_argument if there is
    // no link to the destination or the destination does not exist, and
    // std::logic_error if this node is crashed. Sending to self always works
    // and delivers at the current virtual time.
    void send(node_id to, std::vector<std::byte> payload);
    void send(node_id to, std::string_view payload);

    // Send a typed value to another node: the payload is the 8-byte type
    // tag followed by the serialized value, so the receiver can dispatch
    // on<T> by tag and decode with message::as<T>. Same link/crash rules
    // as the raw sends. A non-serializable T fails at compile time via
    // the serializable concept.
    template <serializable T>
    void send(node_id to, const T& value) {
        std::vector<std::byte> payload;
        detail::append_u64(payload, detail::type_tag<T>());
        serializer<T>::encode(value, payload);
        // The non-template byte overload wins overload resolution over this
        // template (same conversion, non-template preferred), so this
        // delegates to the raw send rather than recursing. The tag travels
        // with the message so the trace can show it at delivery time.
        send_with_tag(to, std::move(payload), detail::type_tag<T>());
    }

    // Schedule a callback at the current virtual time + delay. Callbacks may
    // send messages and schedule further timers. Returns the timer's id
    // (see trace.hxx). Throws std::logic_error if this node is crashed. A
    // timer scheduled before a crash never fires, even if the node restarts
    // before its fire time.
    timer_id after(std::chrono::nanoseconds delay, std::function<void()> callback);

    // Send a typed request and get a handle for configuring retries, a
    // timeout, and the callbacks (see request_handle). The first request
    // fires IMMEDIATELY; the handle's retry_every/timeout_after are
    // measured from when they are called. Request ids are per-node
    // counters. Same link/crash rules as the raw sends: throws
    // std::invalid_argument with no link to the destination, and
    // std::logic_error if this node is crashed.
    template <serializable Req, serializable Resp>
    request_handle<Req, Resp> request(node_id to, const Req& req) {
        // Encode the request payload once; retries reuse the same bytes
        // and the same request id.
        std::vector<std::byte> req_bytes;
        serializer<Req>::encode(req, req_bytes);
        const std::uint64_t id = next_request_id_++;
        auto state = std::make_shared<detail::request_state>();
        state->id = id;
        state->to = to;
        auto on_response = std::make_shared<std::function<void(const Resp&)>>();
        state->is_pending = [this, id] { return pending_.contains(id); };
        state->erase_from_pending = [this, id] { pending_.erase(id); };
        state->resend = [this, id, to, req_tag = detail::type_tag<Req>(), req_bytes,
                         attempt = 1]() mutable {
            // Retries reuse the same request id; the attempt counter lives
            // in this lambda (capturing state would create a reference
            // cycle: state owns resend).
            ++attempt;
            send_request_envelope(id, to, req_tag, req_bytes);
            record_request_sent(id, to, attempt);
        };
        state->arm_timer = [this](std::chrono::nanoseconds d, std::function<void()> cb) {
            after(d, std::move(cb));
        };
        state->record_timeout = [this, id] { record_request_timeout(id); };
        pending_request entry;
        entry.resp_tag = detail::type_tag<Resp>();
        entry.state = state;
        entry.deliver = [on_response](detail::cursor& in) {
            Resp value = serializer<Resp>::decode(in);
            if (in.remaining() != 0) {
                throw decode_error(
                    "simulnetx: decode_error: trailing bytes after the response value");
            }
            if (*on_response) {
                (*on_response)(value);
            }
        };
        pending_.emplace(id, std::move(entry));
        // Fire immediately: safe because time only advances under run_until,
        // so a response can never arrive before the caller attaches
        // on_response.
        send_request_envelope(id, to, detail::type_tag<Req>(), req_bytes);
        record_request_sent(id, to, 1);
        return request_handle<Req, Resp>(state, on_response);
    }

    // Register a server for requests of type Req: each request is decoded,
    // passed to the handler, and the returned Resp is sent back to the
    // requester with the same request id. Registering again for the same
    // Req replaces the previous handler. A request for a type with no
    // server is silently dropped (the client times out). The reply send
    // follows the raw send rules: it throws std::invalid_argument out of
    // run_until if there is no return link to the requester.
    template <serializable Req, serializable Resp>
    void serve(std::function<Resp(const Req&)> handler) {
        serve_handlers_[detail::type_tag<Req>()] =
            [this, handler = std::move(handler)](const message& msg, std::uint64_t id,
                                                 detail::cursor& in) {
                Req req = serializer<Req>::decode(in);
                if (in.remaining() != 0) {
                    throw decode_error(
                        "simulnetx: decode_error: trailing bytes after the request value");
                }
                Resp resp = handler(req);
                std::vector<std::byte> payload;
                detail::append_u64(payload, detail::response_envelope_tag);
                detail::append_u64(payload, id);
                detail::append_u64(payload, detail::type_tag<Resp>());
                serializer<Resp>::encode(resp, payload);
                send_with_tag(msg.from, std::move(payload), detail::type_tag<Resp>());
            };
    }

private:
    friend class network;
    friend class udp::socket;
    friend struct tcp::detail::tcp_connection;
    friend class tcp::socket;
    friend class tcp::acceptor;

    node(network* owner, node_id id, std::string name)
        : owner_(owner), id_(id), name_(std::move(name)) {}

    // One in-flight request, awaiting a response. The deliver callback is
    // type-erased: it decodes the response payload as Resp and invokes the
    // user's on_response.
    struct pending_request {
        std::uint64_t resp_tag = 0;
        std::function<void(detail::cursor&)> deliver;
        std::shared_ptr<detail::request_state> state;
    };

    // Invoked by network when a message is delivered to this node. Returns
    // true if a handler consumed the message (a request/response envelope,
    // a typed handler by tag, or the on_message catch-all); false if no
    // handler matched, in which case network applies the strict-mode rule.
    // Non-const: serving a request sends a reply, and a response removes
    // a pending entry.
    bool deliver(const message& msg);

    // Route a request envelope to the serve handler for its type, and a
    // response envelope to the pending request with its id. A request with
    // no server is silently dropped; a response with no pending entry
    // (late or duplicate) is ignored.
    void dispatch_request(const message& msg);
    void dispatch_response(const message& msg);

    // Send with a declared type tag (0 for raw sends). The tag travels
    // with the message so the trace can show it at delivery time.
    void send_with_tag(node_id to, std::vector<std::byte> payload, std::uint64_t tag);

    // Record typed-layer trace events (the transport events are recorded
    // by network itself; see trace.hxx).
    void record_request_sent(std::uint64_t id, node_id to, unsigned attempt);
    void record_response_received(node_id from, std::uint64_t id);
    void record_request_timeout(std::uint64_t id);

    // Send a request envelope: [request_envelope_tag][id][req_tag][payload].
    void send_request_envelope(std::uint64_t id, node_id to, std::uint64_t req_tag,
                               const std::vector<std::byte>& req_bytes);

    // Send a UDP datagram (see udp.hxx): builds the shim envelope
    // [datagram_envelope_tag][src_port][dst_port][payload] and sends it
    // with the envelope tag, so the trace carries it. Same link/crash
    // rules as the raw sends.
    void send_datagram(node_id to, std::uint16_t src_port, std::uint16_t dst_port,
                       std::vector<std::byte> payload);

    // Send a TCP segment (see tcp.hxx): builds the shim envelope
    // [tcp_envelope_tag][src_port][dst_port][flags][seq][ack][payload]
    // and sends it with the envelope tag, so the trace carries it. Same
    // link/crash rules as the raw sends.
    void send_segment(node_id to, std::uint16_t src_port, std::uint16_t dst_port,
                      std::uint32_t flags, std::uint64_t seq, std::uint64_t ack,
                      std::vector<std::byte> payload);

    // Route a TCP segment to the connection or acceptor for its
    // destination port (via the network's TCP registry). A segment for
    // a port with no connection and no listener gets an RST back, so
    // the sender learns the connection is dead.
    void dispatch_segment(const message& msg);

    // Route a datagram envelope to the socket bound to its destination
    // port (via the network's registry). A datagram for an unbound port
    // is silently dropped.
    void dispatch_datagram(const message& msg);

    // Cancel all pending requests silently, for network::crash: the
    // pending map is cleared, and the retry/timeout timers (node timers)
    // are cancelled by the crash itself, so a dead node's callbacks never
    // fire.
    void cancel_pending_requests();

    // Whether this node has any typed handler registered. Strict mode only
    // applies to nodes that opted into typed dispatch; a node with no
    // typed handlers keeps the v0.1.0 behavior (on_message, or silent
    // ignore when there is no handler at all).
    bool has_typed_handlers() const { return !typed_handlers_.empty(); }

    network* owner_;
    node_id id_;
    std::string name_;
    std::function<void(const message&)> handler_;
    // Typed handlers, keyed by type tag. A raw message whose first 8 bytes
    // happen to equal a registered tag would misdispatch (probability
    // 2^-64 for random bytes); the wire format has no magic prefix, so
    // this is the accepted cost of the 8-byte tag.
    std::map<std::uint64_t, std::function<void(const message&)>> typed_handlers_;
    // Serve handlers, keyed by request type tag. Each takes the message,
    // the request id, and a cursor positioned after the envelope header.
    std::map<std::uint64_t,
             std::function<void(const message&, std::uint64_t, detail::cursor&)>>
        serve_handlers_;
    // In-flight requests, keyed by request id (per-node counter).
    std::map<std::uint64_t, pending_request> pending_;
    std::uint64_t next_request_id_{0};
};

}  // namespace simulnetx
