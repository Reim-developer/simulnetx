#include "simulnetx/udp.hxx"

#include <algorithm>
#include <cstring>
#include <stdexcept>

namespace simulnetx::udp {

std::string to_string(const endpoint& ep) {
    return "node#" + std::to_string(ep.node) + ":" + std::to_string(ep.port);
}

socket::socket(network& net, node& node) : net_(&net), node_(&node) {}

socket::~socket() { close(); }

socket::socket(socket&& other) noexcept : net_(other.net_), node_(other.node_) {
    adopt(std::move(other));
}

socket& socket::operator=(socket&& other) noexcept {
    if (this != &other) {
        close();
        net_ = other.net_;
        node_ = other.node_;
        adopt(std::move(other));
    }
    return *this;
}

void socket::adopt(socket&& other) noexcept {
    open_ = other.open_;
    bound_ = other.bound_;
    port_ = other.port_;
    connected_ = other.connected_;
    peer_ = other.peer_;
    queue_ = std::move(other.queue_);
    recv_buffer_ = other.recv_buffer_;
    recv_completion_ = std::move(other.recv_completion_);
    recv_pending_ = other.recv_pending_;
    if (bound_) {
        net_->unregister_socket(node_->id(), port_);
        net_->register_socket(node_->id(), port_, this);
    }
    other.open_ = false;
    other.bound_ = false;
    other.connected_ = false;
    other.recv_pending_ = false;
    other.recv_completion_ = nullptr;
}

void socket::bind(std::uint16_t port) {
    if (!open_) {
        throw std::logic_error("simulnetx: bind on a closed socket");
    }
    if (bound_) {
        throw std::logic_error("simulnetx: socket is already bound");
    }
    port_ = net_->register_socket(node_->id(), port, this);
    bound_ = true;
}

endpoint socket::local_endpoint() const {
    if (!bound_) {
        throw std::logic_error("simulnetx: local_endpoint on an unbound socket");
    }
    return endpoint{node_->id(), port_};
}

void socket::close() {
    if (!open_) {
        return;
    }
    open_ = false;
    if (bound_) {
        net_->unregister_socket(node_->id(), port_);
        bound_ = false;
    }
    queue_.clear();
    cancel_pending_receive();
}

void socket::connect(endpoint peer) {
    if (!open_) {
        throw std::logic_error("simulnetx: connect on a closed socket");
    }
    peer_ = peer;
    connected_ = true;
}

void socket::async_send_to(std::span<const std::byte> data, endpoint peer,
                           send_handler handler) {
    if (!open_) {
        throw std::logic_error("simulnetx: async_send_to on a closed socket");
    }
    ensure_bound();
    std::vector<std::byte> payload(data.begin(), data.end());
    // Throws synchronously on no link / unknown node / crashed sender;
    // the completion is only scheduled after the send succeeded.
    node_->send_datagram(peer.node, port_, peer.port, std::move(payload));
    const std::size_t n = data.size();
    net_->schedule_silent([handler = std::move(handler), n] {
        handler(std::error_code{}, n);
    });
}

void socket::async_send(std::span<const std::byte> data, send_handler handler) {
    if (!connected_) {
        throw std::logic_error("simulnetx: async_send on an unconnected socket");
    }
    async_send_to(data, peer_, std::move(handler));
}

void socket::async_receive_from(std::span<std::byte> buffer, receive_handler handler) {
    if (!open_) {
        throw std::logic_error("simulnetx: async_receive_from on a closed socket");
    }
    if (!bound_) {
        throw std::logic_error("simulnetx: async_receive_from on an unbound socket");
    }
    if (recv_pending_) {
        throw std::logic_error("simulnetx: a receive is already pending on this socket");
    }
    if (!queue_.empty()) {
        auto [sender, payload] = std::move(queue_.front());
        queue_.pop_front();
        const std::size_t n = std::min(buffer.size(), payload.size());
        if (n > 0) {
            std::memcpy(buffer.data(), payload.data(), n);
        }
        // Bytes are copied now; only the handler invocation is deferred,
        // so it never fires synchronously from this call.
        fire_later(std::move(handler), std::error_code{}, n, sender);
        return;
    }
    recv_buffer_ = buffer;
    recv_completion_ = std::move(handler);
    recv_pending_ = true;
}

void socket::async_receive(std::span<std::byte> buffer,
                           connected_receive_handler handler) {
    if (!connected_) {
        throw std::logic_error("simulnetx: async_receive on an unconnected socket");
    }
    async_receive_from(buffer,
                       [handler = std::move(handler)](std::error_code ec, std::size_t n,
                                                      endpoint /*sender*/) {
                           handler(ec, n);
                       });
}

void socket::cancel() { cancel_pending_receive(); }

void socket::on_datagram(endpoint sender, std::vector<std::byte> payload) {
    if (!recv_pending_) {
        queue_.emplace_back(sender, std::move(payload));
        return;
    }
    const std::size_t n = std::min(recv_buffer_.size(), payload.size());
    if (n > 0) {
        std::memcpy(recv_buffer_.data(), payload.data(), n);
    }
    recv_pending_ = false;
    completion_fn completion = std::move(recv_completion_);
    recv_completion_ = nullptr;
    // We are inside run_until (delivery happens there), so the handler
    // fires right here - same as the request engine's on_response.
    completion(std::error_code{}, n, sender);
}

void socket::on_node_crashed() {
    queue_.clear();
    cancel_pending_receive();
}

void socket::ensure_bound() {
    if (bound_) {
        return;
    }
    port_ = net_->register_socket(node_->id(), 0, this);
    bound_ = true;
}

void socket::cancel_pending_receive() {
    if (!recv_pending_) {
        return;
    }
    recv_pending_ = false;
    completion_fn completion = std::move(recv_completion_);
    recv_completion_ = nullptr;
    fire_later(std::move(completion),
               std::make_error_code(std::errc::operation_canceled), 0, endpoint{});
}

void socket::fire_later(completion_fn completion, std::error_code ec, std::size_t n,
                        endpoint sender) {
    net_->schedule_silent([completion = std::move(completion), ec, n, sender] {
        completion(ec, n, sender);
    });
}

}  // namespace simulnetx::udp
