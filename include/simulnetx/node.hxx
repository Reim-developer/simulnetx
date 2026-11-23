#pragma once

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <map>
#include <string>
#include <string_view>
#include <vector>

#include "simulnetx/message.hxx"
#include "simulnetx/serializer.hxx"

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
        // delegates to the raw send rather than recursing.
        send(to, std::move(payload));
    }

    // Schedule a callback at the current virtual time + delay. Callbacks may
    // send messages and schedule further timers. Throws std::logic_error if
    // this node is crashed. A timer scheduled before a crash never fires,
    // even if the node restarts before its fire time.
    void after(std::chrono::nanoseconds delay, std::function<void()> callback);

private:
    friend class network;

    node(network* owner, node_id id, std::string name)
        : owner_(owner), id_(id), name_(std::move(name)) {}

    // Invoked by network when a message is delivered to this node. Returns
    // true if a handler consumed the message (a typed handler by tag, or
    // the on_message catch-all); false if no handler matched, in which
    // case network applies the strict-mode rule.
    bool deliver(const message& msg) const {
        if (msg.payload.size() >= sizeof(std::uint64_t)) {
            const std::uint64_t tag = detail::read_u64(msg.payload.data());
            const auto it = typed_handlers_.find(tag);
            if (it != typed_handlers_.end()) {
                it->second(msg);
                return true;
            }
        }
        if (handler_) {
            handler_(msg);
            return true;
        }
        return false;
    }

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
};

}  // namespace simulnetx
