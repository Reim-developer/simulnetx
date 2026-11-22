#include "simulnetx/node.hxx"

#include <utility>

#include "simulnetx/network.hxx"

namespace simulnetx {

void node::on_message(std::function<void(const message&)> handler) {
    handler_ = std::move(handler);
}

void node::send(node_id to, std::vector<std::byte> payload) {
    owner_->send_from(id_, to, std::move(payload));
}

void node::send(node_id to, std::string_view payload) {
    std::vector<std::byte> bytes;
    bytes.reserve(payload.size());
    for (const char c : payload) {
        bytes.push_back(static_cast<std::byte>(c));
    }
    send(to, std::move(bytes));
}

void node::after(std::chrono::nanoseconds delay, std::function<void()> callback) {
    owner_->schedule_node_timer(id_, delay, std::move(callback));
}

}  // namespace simulnetx
