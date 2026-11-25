#include "simulnetx/udp.hxx"

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
    queue_ = std::move(other.queue_);
    if (bound_) {
        net_->unregister_socket(node_->id(), port_);
        net_->register_socket(node_->id(), port_, this);
    }
    other.open_ = false;
    other.bound_ = false;
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
}

void socket::on_datagram(endpoint sender, std::vector<std::byte> payload) {
    queue_.emplace_back(sender, std::move(payload));
}

}  // namespace simulnetx::udp
