#include "simulnetx/scenario.hxx"

#include <stdexcept>
#include <utility>

namespace simulnetx {

scenario& scenario::seed(std::uint64_t s) {
    if (net_ != nullptr) {
        throw std::logic_error("simulnetx: scenario '" + name_ +
                               "': seed() called after the network was created");
    }
    seed_ = s;
    return *this;
}

scenario& scenario::link(std::string_view a, std::string_view b, link_config cfg) {
    const node_id a_id = resolve_node(a).id();
    const node_id b_id = resolve_node(b).id();
    ensure_network().link(a_id, b_id, cfg);
    return *this;
}

simulnetx::node& scenario::node(std::string_view name) {
    return resolve_node(name);
}

simulnetx::network& scenario::network() {
    return ensure_network();
}

scenario& scenario::run_until(std::chrono::nanoseconds t) {
    ensure_network().run_until(t);
    return *this;
}

simulnetx::network& scenario::ensure_network() {
    if (net_ == nullptr) {
        net_ = std::make_unique<simulnetx::network>(seed_);
    }
    return *net_;
}

void scenario::add_node_named(std::string_view name) {
    if (nodes_.contains(name)) {
        throw std::invalid_argument("simulnetx: scenario '" + name_ + "': duplicate node '" +
                                    std::string(name) + "'");
    }
    simulnetx::node& created = ensure_network().add_node(std::string(name));
    nodes_.emplace(std::string(name), &created);
}

simulnetx::node& scenario::resolve_node(std::string_view name) {
    const auto it = nodes_.find(name);
    if (it == nodes_.end()) {
        throw std::invalid_argument("simulnetx: scenario '" + name_ + "': unknown node '" +
                                    std::string(name) + "'");
    }
    return *it->second;
}

}  // namespace simulnetx
