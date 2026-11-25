#pragma once

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <functional>
#include <span>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

#include "simulnetx/message.hxx"
#include "simulnetx/network.hxx"
#include "simulnetx/node.hxx"

namespace simulnetx::udp {

// An address in the sim: a node plus a port. Node ids are addresses;
// there is no IP layer underneath.
struct endpoint {
    node_id node{};
    std::uint16_t port{};

    friend bool operator==(const endpoint&, const endpoint&) = default;
};

// "node#3:8080". Names live in the network, so only ids print here.
std::string to_string(const endpoint& ep);

// A UDP socket bound to one node of one network. Async Asio-style API,
// minimal: no io_context (the network IS the io_context - run_until is
// io_context::run), no strands (the sim is single-threaded by
// construction), no buffer machinery (std::span instead).
//
// INVARIANT: no completion handler ever runs outside run_until. Send
// completions fire as zero-delay sim events (via the network's private
// schedule_silent path, so they leave no timer_fired event and consume
// no timer id); receives fire when their datagram is delivered, which is
// also inside run_until.
//
// ERROR SPLIT: the sync API throws like the rest of simulnetx
// (std::invalid_argument for bad arguments, std::logic_error for state
// violations); async completions report via std::error_code. Send-path
// errors (no link, unknown node, crashed sender) throw synchronously
// out of async_send_to / async_send - exactly like node::send - and the
// handler never fires. (Cancellation arrives with the next commit;
// it reports std::errc::operation_canceled.)
//
// Non-copyable, movable (a move transfers the binding, the queued
// datagrams and the pending receive, and updates the registry entry).
// Must not outlive the network - same rule as node references.
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
    // network assigns the lowest free port from 49152 up, per node.
    // Throws std::logic_error if already bound, or if the port is
    // already bound on this node. The same port on a DIFFERENT node is
    // fine - the registry is keyed by (node, port).
    void bind(std::uint16_t port);

    // This socket's address. Throws std::logic_error if unbound.
    endpoint local_endpoint() const;

    // False after close(). close() is idempotent: it unregisters the
    // port and discards queued datagrams.
    bool is_open() const { return open_; }
    void close();

    // Connected UDP: remember a peer for async_send/async_receive.
    // Sends nothing - UDP connect is local only. Re-connecting
    // replaces the peer (Asio-faithful). Allowed on an unbound socket;
    // the first send binds an ephemeral port (see async_send_to).
    void connect(endpoint peer);
    bool is_connected() const { return connected_; }

    // Send one datagram (one sim message). The data is COPIED into the
    // envelope, so the span need not outlive the call. On success the
    // handler fires as a zero-delay event with (success, bytes sent).
    // Throws synchronously (handler never fires) with no link to the
    // peer, an unknown peer node, or a crashed sender - the node::send
    // rules. Sending from an unbound socket implicitly binds an
    // ephemeral port first (Asio rule), visible via local_endpoint()
    // afterwards. A send handler's error is always clear in v0.6.0: no
    // async send failure exists yet.
    using send_handler = std::function<void(std::error_code, std::size_t)>;
    void async_send_to(std::span<const std::byte> data, endpoint peer,
                       send_handler handler);
    void async_send(std::span<const std::byte> data, send_handler handler);
    // async_send throws std::logic_error if not connected.

    // Receive one datagram. The buffer is CALLER-OWNED and must
    // outlive the operation (Asio rule). A datagram larger than the
    // buffer is truncated to the buffer size, the rest discarded
    // (Asio rule). At most ONE pending receive per socket; a second
    // async_receive* while one is pending throws std::logic_error -
    // a deliberate deviation from Asio, which allows several. One
    // waiter + one FIFO queue keeps delivery order trivially
    // deterministic; code that needs concurrency uses several
    // sockets. If a datagram is already queued, the handler fires via
    // a zero-delay event - never synchronously. Throws
    // std::logic_error on an unbound socket (nothing can address you)
    // and, for async_receive, if not connected. On an error
    // completion the sender endpoint is default-constructed (it is
    // only meaningful on success).
    using receive_handler =
        std::function<void(std::error_code, std::size_t, endpoint)>;
    using connected_receive_handler =
        std::function<void(std::error_code, std::size_t)>;
    void async_receive_from(std::span<std::byte> buffer, receive_handler handler);
    void async_receive(std::span<std::byte> buffer, connected_receive_handler handler);

private:
    // Qualified: an unqualified `friend class network;` here would
    // declare a NEW class simulnetx::udp::network (friend lookup for an
    // elaborated-type-specifier considers only the innermost enclosing
    // namespace) instead of befriending simulnetx::network.
    friend class ::simulnetx::network;

    // One completion, type-erased: async_receive wraps its handler to
    // drop the sender endpoint.
    using completion_fn = std::function<void(std::error_code, std::size_t, endpoint)>;

    // Called by network when a datagram arrives for this socket's port.
    // Completes the pending receive synchronously (we are inside
    // run_until) or queues the datagram.
    void on_datagram(endpoint sender, std::vector<std::byte> payload);

    // Move-transfer helper, shared by the move constructor and move
    // assignment: takes other's state, repoints the registry entry,
    // and leaves other closed with no pending receive.
    void adopt(socket&& other) noexcept;

    // Bind an ephemeral port if not already bound.
    void ensure_bound();

    // Schedule a completion as a zero-delay sim event, so it fires
    // during run_until and never synchronously from the caller's call.
    // Uses schedule_silent (not after(0ns)): a completion is not a
    // user-scheduled timer, so it must not record a timer_fired event
    // or consume a timer id.
    void fire_later(completion_fn completion, std::error_code ec, std::size_t n,
                    endpoint sender);

    network* net_;
    node* node_;
    bool open_{true};
    bool bound_{false};
    std::uint16_t port_{0};
    bool connected_{false};
    endpoint peer_{};
    std::deque<std::pair<endpoint, std::vector<std::byte>>> queue_;
    std::span<std::byte> recv_buffer_{};
    completion_fn recv_completion_;
    bool recv_pending_{false};
};

}  // namespace simulnetx::udp
