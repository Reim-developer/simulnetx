#include "simulnetx/scenario.hxx"

#include <algorithm>
#include <stdexcept>
#include <string>
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

scenario& scenario::link_directed(std::string_view a, std::string_view b, link_config cfg) {
    const node_id a_id = resolve_node(a).id();
    const node_id b_id = resolve_node(b).id();
    ensure_network().link_directed(a_id, b_id, cfg);
    return *this;
}

scenario& scenario::mesh(link_config cfg) {
    if (node_order_.size() < 2) {
        throw std::logic_error("simulnetx: scenario '" + name_ +
                               "': mesh() needs at least 2 nodes, has " +
                               std::to_string(node_order_.size()));
    }
    simulnetx::network& net = ensure_network();
    for (std::size_t i = 0; i < node_order_.size(); ++i) {
        for (std::size_t j = i + 1; j < node_order_.size(); ++j) {
            net.link(node_order_[i]->id(), node_order_[j]->id(), cfg);
        }
    }
    return *this;
}

scenario& scenario::mesh(std::vector<std::string_view> names, link_config cfg) {
    if (names.size() < 2) {
        throw std::logic_error("simulnetx: scenario '" + name_ +
                               "': mesh() needs at least 2 nodes, has " +
                               std::to_string(names.size()));
    }
    std::vector<node_id> ids;
    ids.reserve(names.size());
    for (const std::string_view name : names) {
        ids.push_back(resolve_node(name).id());
    }
    simulnetx::network& net = ensure_network();
    for (std::size_t i = 0; i < ids.size(); ++i) {
        for (std::size_t j = i + 1; j < ids.size(); ++j) {
            net.link(ids[i], ids[j], cfg);
        }
    }
    return *this;
}

scenario& scenario::star(std::string_view hub, link_config cfg) {
    simulnetx::node& hub_node = resolve_node(hub);
    if (node_order_.size() < 2) {
        throw std::logic_error("simulnetx: scenario '" + name_ +
                               "': star() needs a hub plus at least 1 other node, has " +
                               std::to_string(node_order_.size()) + " node(s)");
    }
    simulnetx::network& net = ensure_network();
    for (const simulnetx::node* other : node_order_) {
        if (other != &hub_node) {
            net.link(hub_node.id(), other->id(), cfg);
        }
    }
    return *this;
}

scenario& scenario::chain(link_config cfg) {
    if (node_order_.size() < 2) {
        throw std::logic_error("simulnetx: scenario '" + name_ +
                               "': chain() needs at least 2 nodes, has " +
                               std::to_string(node_order_.size()));
    }
    simulnetx::network& net = ensure_network();
    for (std::size_t i = 0; i + 1 < node_order_.size(); ++i) {
        net.link(node_order_[i]->id(), node_order_[i + 1]->id(), cfg);
    }
    return *this;
}

simulnetx::node& scenario::node(std::string_view name) {
    return resolve_node(name);
}

simulnetx::network& scenario::network() {
    return ensure_network();
}

const std::vector<trace_event>& scenario::trace() {
    return ensure_network().trace();
}

std::string scenario::dump_trace() {
    return ensure_network().dump_trace();
}

scenario& scenario::run_until(std::chrono::nanoseconds t) {
    ensure_network().run_until(t);
    return *this;
}

scenario& scenario::partition_at(std::chrono::nanoseconds t,
                                 std::initializer_list<std::string_view> group_a,
                                 std::initializer_list<std::string_view> group_b) {
    std::vector<node_id> a_ids;
    std::vector<node_id> b_ids;
    a_ids.reserve(group_a.size());
    b_ids.reserve(group_b.size());
    for (const std::string_view name : group_a) {
        a_ids.push_back(resolve_node(name).id());
    }
    for (const std::string_view name : group_b) {
        b_ids.push_back(resolve_node(name).id());
    }
    for (const node_id id : a_ids) {
        if (std::find(b_ids.begin(), b_ids.end(), id) != b_ids.end()) {
            throw std::invalid_argument("simulnetx: scenario '" + name_ +
                                        "': node appears in both partition groups");
        }
    }
    schedule_at(t, [net = &ensure_network(), a = std::move(a_ids), b = std::move(b_ids)] {
        net->partition(a, b);
    });
    return *this;
}

scenario& scenario::heal_at(std::chrono::nanoseconds t) {
    schedule_at(t, [net = &ensure_network()] { net->heal(); });
    return *this;
}

scenario& scenario::crash_at(std::chrono::nanoseconds t, std::string_view name) {
    const node_id id = resolve_node(name).id();
    schedule_at(t, [net = &ensure_network(), id] { net->crash(id); });
    return *this;
}

scenario& scenario::restart_at(std::chrono::nanoseconds t, std::string_view name) {
    const node_id id = resolve_node(name).id();
    schedule_at(t, [net = &ensure_network(), id] { net->restart(id); });
    return *this;
}

scenario& scenario::send_at(std::chrono::nanoseconds t, std::string_view from, std::string_view to,
                            std::string_view payload) {
    std::vector<std::byte> bytes;
    bytes.reserve(payload.size());
    for (const char c : payload) {
        bytes.push_back(static_cast<std::byte>(c));
    }
    return send_at(t, from, to, std::move(bytes));
}

scenario& scenario::send_at(std::chrono::nanoseconds t, std::string_view from, std::string_view to,
                            std::vector<std::byte> payload) {
    simulnetx::node& from_node = resolve_node(from);
    const node_id to_id = resolve_node(to).id();
    schedule_at(t, [from_ptr = &from_node, to_id, bytes = std::move(payload)] {
        from_ptr->send(to_id, bytes);
    });
    return *this;
}

scenario& scenario::on_message(std::string_view name,
                               std::function<void(const message&)> handler) {
    resolve_node(name).on_message(std::move(handler));
    return *this;
}

scenario& scenario::strict_tags() {
    strict_tags_ = true;
    if (net_ != nullptr) {
        net_->set_strict_tags(true);
    }
    return *this;
}

scenario& scenario::lenient_tags() {
    strict_tags_ = false;
    if (net_ != nullptr) {
        net_->set_strict_tags(false);
    }
    return *this;
}

scenario& scenario::expect_sent(std::size_t n) {
    check([this, n] { ensure_network().sent_count().equal(n); });
    return *this;
}

scenario& scenario::expect_delivered(std::size_t n) {
    check([this, n] { ensure_network().delivered_count().equal(n); });
    return *this;
}

scenario& scenario::expect_dropped(std::size_t n) {
    check([this, n] { ensure_network().dropped_count().equal(n); });
    return *this;
}

scenario& scenario::expect_sent_between(std::size_t lo, std::size_t hi) {
    check([this, lo, hi] { ensure_network().sent_count().between(lo, hi); });
    return *this;
}

scenario& scenario::expect_delivered_between(std::size_t lo, std::size_t hi) {
    check([this, lo, hi] { ensure_network().delivered_count().between(lo, hi); });
    return *this;
}

scenario& scenario::expect_dropped_between(std::size_t lo, std::size_t hi) {
    check([this, lo, hi] { ensure_network().dropped_count().between(lo, hi); });
    return *this;
}

scenario& scenario::expect_events(event_kind kind, std::size_t n) {
    check([this, kind, n] { ensure_network().event_count(kind).equal(n); });
    return *this;
}

scenario& scenario::expect_events_between(event_kind kind, std::size_t lo, std::size_t hi) {
    check([this, kind, lo, hi] { ensure_network().event_count(kind).between(lo, hi); });
    return *this;
}

void scenario::schedule_at(std::chrono::nanoseconds t, std::function<void()> callback) {
    simulnetx::network& net = ensure_network();
    if (t < net.now()) {
        throw std::invalid_argument("simulnetx: scenario '" + name_ +
                                    "': scheduled time is in the past");
    }
    // Network-level timer: needs no node to exist, and survives node
    // crashes (a crash_at scheduled before a crash still fires, and a
    // restart_at scheduled during a crash still fires).
    net.schedule_at(t, std::move(callback));
}

simulnetx::network& scenario::ensure_network() {
    if (net_ == nullptr) {
        net_ = std::make_unique<simulnetx::network>(seed_);
        net_->set_strict_tags(strict_tags_);
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
    node_order_.push_back(&created);
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
