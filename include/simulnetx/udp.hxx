#pragma once

#include <cstddef>
#include <cstdint>
#include <deque>
#include <string>
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
// Non-copyable, movable (a move transfers the binding and the queued
// datagrams, and updates the registry entry). Must not outlive the
// network - same rule as node references.
//
// This commit carries binding only; async send/receive arrive next.
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

private:
    // Qualified: an unqualified `friend class network;` here would
    // declare a NEW class simulnetx::udp::network (friend lookup for an
    // elaborated-type-specifier considers only the innermost enclosing
    // namespace) instead of befriending simulnetx::network.
    friend class ::simulnetx::network;

    // Called by network when a datagram arrives for this socket's port.
    // Queues it; the receive API (next commit) drains the queue.
    void on_datagram(endpoint sender, std::vector<std::byte> payload);

    // Move-transfer helper, shared by the move constructor and move
    // assignment: takes other's state, repoints the registry entry,
    // and leaves other closed.
    void adopt(socket&& other) noexcept;

    network* net_;
    node* node_;
    bool open_{true};
    bool bound_{false};
    std::uint16_t port_{0};
    std::deque<std::pair<endpoint, std::vector<std::byte>>> queue_;
};

}  // namespace simulnetx::udp
