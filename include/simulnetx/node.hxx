#pragma once

#include <chrono>
#include <cstddef>
#include <functional>
#include <string>
#include <string_view>
#include <vector>

#include "simulnetx/message.hxx"

namespace simulnetx {

class network;

// One endpoint in a simulated network. Nodes are owned by network; references
// returned by network::add_node stay valid for the lifetime of the network.
class node {
public:
    node(const node&) = delete;
    node& operator=(const node&) = delete;
    node(node&&) = delete;
    node& operator=(node&&) = delete;

    node_id id() const { return id_; }
    const std::string& name() const { return name_; }

    // Set the handler invoked for each delivered message. Replaces any
    // previously set handler. A node with no handler silently ignores
    // delivered messages (they still count as delivered).
    void on_message(std::function<void(const message&)> handler);

    // Send a message to another node. Throws std::invalid_argument if there is
    // no link to the destination or the destination does not exist. Sending to
    // self always works and delivers at the current virtual time.
    void send(node_id to, std::vector<std::byte> payload);
    void send(node_id to, std::string_view payload);

    // Schedule a callback at the current virtual time + delay. Callbacks may
    // send messages and schedule further timers.
    void after(std::chrono::nanoseconds delay, std::function<void()> callback);

private:
    friend class network;

    node(network* owner, node_id id, std::string name)
        : owner_(owner), id_(id), name_(std::move(name)) {}

    // Invoked by network when a message is delivered to this node.
    void deliver(const message& msg) const {
        if (handler_) {
            handler_(msg);
        }
    }

    network* owner_;
    node_id id_;
    std::string name_;
    std::function<void(const message&)> handler_;
};

}  // namespace simulnetx
