#include "simulnetx/network.hxx"

#include <stdexcept>
#include <utility>

#include "simulnetx/errors.hxx"
#include "simulnetx/serializer.hxx"

namespace simulnetx {

namespace {

void validate_config(const link_config& cfg) {
    if (cfg.latency.count() < 0 || cfg.jitter.count() < 0) {
        throw std::invalid_argument("simulnetx: link latency and jitter must be non-negative");
    }
    for (const double rate : {cfg.loss_rate, cfg.dup_rate, cfg.reorder_rate}) {
        if (!(rate >= 0.0 && rate <= 1.0)) {
            throw std::invalid_argument("simulnetx: link rates must be in [0, 1]");
        }
    }
}

}  // namespace

node& network::add_node(std::string name) {
    const auto id = static_cast<node_id>(nodes_.size());
    // Direct new: std::make_unique cannot call node's private constructor
    // (friendship does not extend to it); network is a friend, so new works.
    nodes_.push_back(std::unique_ptr<node>(new node(this, id, std::move(name))));
    generations_.push_back(0);
    return *nodes_.back();
}

void network::link(node_id a, node_id b, link_config cfg) {
    if (a >= nodes_.size() || b >= nodes_.size()) {
        throw std::invalid_argument("simulnetx: link references an unknown node id");
    }
    if (a == b) {
        throw std::invalid_argument("simulnetx: cannot link a node to itself (self-send needs no link)");
    }
    validate_config(cfg);
    // Links are stored keyed by ordered pair, so a symmetric link sets both
    // directions. Sampling reads only the (from, to) entry, so the storage
    // change does not alter RNG behavior for symmetric links.
    links_[{a, b}] = cfg;
    links_[{b, a}] = cfg;
}

void network::link_directed(node_id from, node_id to, link_config cfg) {
    if (from >= nodes_.size() || to >= nodes_.size()) {
        throw std::invalid_argument("simulnetx: link_directed references an unknown node id");
    }
    if (from == to) {
        throw std::invalid_argument(
            "simulnetx: cannot link a node to itself (self-send needs no link)");
    }
    validate_config(cfg);
    links_[{from, to}] = cfg;
}

void network::partition(std::vector<node_id> group_a, std::vector<node_id> group_b) {
    const std::unordered_set<node_id> a(group_a.begin(), group_a.end());
    const std::unordered_set<node_id> b(group_b.begin(), group_b.end());
    for (const node_id id : a) {
        if (id >= nodes_.size()) {
            throw std::invalid_argument("simulnetx: partition references an unknown node id");
        }
        if (b.contains(id)) {
            throw std::invalid_argument("simulnetx: node appears in both partition groups");
        }
    }
    for (const node_id id : b) {
        if (id >= nodes_.size()) {
            throw std::invalid_argument("simulnetx: partition references an unknown node id");
        }
    }
    partition_a_ = a;
    partition_b_ = b;
    record(trace_event{.kind = event_kind::partitioned});
}

void network::heal() {
    if (partition_a_.empty() && partition_b_.empty()) {
        return;  // No partition active: a no-op, like restarting a live node.
    }
    partition_a_.clear();
    partition_b_.clear();
    record(trace_event{.kind = event_kind::healed});
}

void network::crash(node_id id) {
    if (id >= nodes_.size()) {
        throw std::invalid_argument("simulnetx: crash references an unknown node id");
    }
    if (crashed_.contains(id)) {
        return;
    }
    crashed_.insert(id);
    // Bumping the generation cancels the node's pending timers: their
    // callbacks check the generation at fire time and no-op on mismatch.
    // No RNG draws are consumed.
    ++generations_[id];
    // Pending requests are cancelled silently: a dead node's callbacks
    // must not fire, and its retry/timeout timers are already cancelled
    // by the generation bump above.
    nodes_[id]->cancel_pending_requests();
    record(trace_event{.kind = event_kind::node_crashed, .node = id});
}

void network::restart(node_id id) {
    if (id >= nodes_.size()) {
        throw std::invalid_argument("simulnetx: restart references an unknown node id");
    }
    if (!crashed_.contains(id)) {
        return;  // Restarting a live node is a no-op.
    }
    crashed_.erase(id);
    record(trace_event{.kind = event_kind::node_restarted, .node = id});
}

bool network::is_crashed(node_id id) const {
    if (id >= nodes_.size()) {
        throw std::invalid_argument("simulnetx: is_crashed references an unknown node id");
    }
    return crashed_.contains(id);
}

void network::run_until(std::chrono::nanoseconds t) {
    if (t < now_) {
        throw std::invalid_argument("simulnetx: run_until target is before the current virtual time");
    }
    while (!events_.empty() && events_.top().time <= t) {
        const scheduled_event event = events_.top();
        events_.pop();
        now_ = event.time;
        event.action();
    }
    now_ = t;
}

void network::run_for(std::chrono::nanoseconds d) {
    if (d.count() < 0) {
        throw std::invalid_argument("simulnetx: run_for duration must be non-negative");
    }
    run_until(now_ + d);
}

timer_id network::after(std::chrono::nanoseconds delay, std::function<void()> callback) {
    if (delay.count() < 0) {
        throw std::invalid_argument("simulnetx: timer delay must be non-negative");
    }
    const timer_id id = next_timer_id_++;
    schedule(now_ + delay, [this, id, cb = std::move(callback)] {
        record(trace_event{.kind = event_kind::timer_fired, .timer = id, .node = no_node});
        cb();
    });
    return id;
}

timer_id network::schedule_at(std::chrono::nanoseconds t, std::function<void()> callback) {
    if (t < now_) {
        throw std::invalid_argument("simulnetx: schedule_at time is before the current virtual time");
    }
    const timer_id id = next_timer_id_++;
    schedule(t, [this, id, cb = std::move(callback)] {
        record(trace_event{.kind = event_kind::timer_fired, .timer = id, .node = no_node});
        cb();
    });
    return id;
}

expectation<std::size_t> network::sent_count() const {
    return expectation<std::size_t>(sent_, "sent_count", detail::sim_context{seed_, now_});
}

expectation<std::size_t> network::delivered_count() const {
    return expectation<std::size_t>(delivered_, "delivered_count", detail::sim_context{seed_, now_});
}

expectation<std::size_t> network::dropped_count() const {
    return expectation<std::size_t>(dropped_, "dropped_count", detail::sim_context{seed_, now_});
}

void network::send_from(node_id from, node_id to, std::vector<std::byte> payload,
                        std::uint64_t tag) {
    if (to >= nodes_.size()) {
        throw std::invalid_argument("simulnetx: send to unknown node id");
    }
    // A dead process cannot send, not even to itself.
    if (crashed_.contains(from)) {
        throw std::logic_error("simulnetx: cannot send from a crashed node");
    }

    // Self-send: no link needed, delivers at the current virtual time.
    if (from == to) {
        ++sent_;
        const std::size_t bytes = payload.size();
        message msg{from, to, std::move(payload)};
        msg.tag = tag;
        record(trace_event{.kind = event_kind::sent,
                           .from = from,
                           .to = to,
                           .bytes = bytes,
                           .tag = tag});
        schedule(now_, [this, msg = std::move(msg)] { deliver(msg); });
        return;
    }

    const auto it = links_.find({from, to});
    if (it == links_.end()) {
        throw std::invalid_argument("simulnetx: no link between the sending and receiving nodes");
    }
    const link_config cfg = it->second;
    const std::size_t bytes = payload.size();
    ++sent_;
    record(trace_event{.kind = event_kind::sent,
                       .from = from,
                       .to = to,
                       .bytes = bytes,
                       .tag = tag});

    // Partitioned at send time: the message never enters the network, and no
    // RNG draws are consumed for it.
    if (partitioned_between(from, to)) {
        ++dropped_;
        record(trace_event{.kind = event_kind::dropped,
                           .from = from,
                           .to = to,
                           .bytes = bytes,
                           .tag = tag,
                           .reason = drop_reason::partition});
        return;
    }

    // Destination crashed at send time: same treatment as a partition, no
    // RNG draws consumed.
    if (crashed_.contains(to)) {
        ++dropped_;
        record(trace_event{.kind = event_kind::dropped,
                           .from = from,
                           .to = to,
                           .bytes = bytes,
                           .tag = tag,
                           .reason = drop_reason::crashed});
        return;
    }

    // Sampling order is fixed and documented (DESIGN.md): loss, jitter, dup,
    // reorder. A draw happens only when the corresponding feature is active,
    // so enabling a feature does not change the stream of the others.
    if (cfg.loss_rate > 0.0 && rng_.next_double() < cfg.loss_rate) {
        ++dropped_;
        record(trace_event{.kind = event_kind::dropped,
                           .from = from,
                           .to = to,
                           .bytes = bytes,
                           .tag = tag,
                           .reason = drop_reason::loss});
        return;
    }

    std::chrono::nanoseconds extra_delay = sample_delay(cfg.jitter);

    const bool duplicate = cfg.dup_rate > 0.0 && rng_.next_double() < cfg.dup_rate;

    bool reordered = false;
    if (cfg.reorder_rate > 0.0 && rng_.next_double() < cfg.reorder_rate) {
        // Reorder adds a second jitter-range delay, so the message may arrive
        // after a later-sent one. No visible effect when jitter is zero.
        extra_delay += sample_delay(cfg.jitter);
        reordered = true;
    }

    if (duplicate) {
        record(trace_event{.kind = event_kind::duplicated,
                           .from = from,
                           .to = to,
                           .bytes = bytes,
                           .tag = tag});
    }
    if (reordered) {
        record(trace_event{.kind = event_kind::reordered,
                           .from = from,
                           .to = to,
                           .bytes = bytes,
                           .tag = tag});
    }

    const std::chrono::nanoseconds arrival = now_ + cfg.latency + extra_delay;
    message msg{from, to, std::move(payload)};
    msg.tag = tag;
    if (duplicate) {
        // The second copy is identical and arrives 1ns after the first.
        // (Built before msg moves into the delivery lambda below.)
        message copy{from, to, msg.payload};
        copy.tag = tag;
        schedule(arrival + std::chrono::nanoseconds{1},
                 [this, copy = std::move(copy)] { deliver(copy); });
    }
    schedule(arrival, [this, msg = std::move(msg)] { deliver(msg); });
}

timer_id network::schedule_node_timer(node_id owner, std::chrono::nanoseconds delay,
                                      std::function<void()> callback) {
    if (delay.count() < 0) {
        throw std::invalid_argument("simulnetx: timer delay must be non-negative");
    }
    if (crashed_.contains(owner)) {
        throw std::logic_error("simulnetx: cannot schedule a timer on a crashed node");
    }
    const std::uint64_t generation = generations_[owner];
    const timer_id id = next_timer_id_++;
    schedule(now_ + delay, [this, owner, generation, id, cb = std::move(callback)] {
        // The node crashed (and possibly restarted) since this timer was
        // scheduled: the timer belonged to the dead incarnation and is gone.
        if (generations_[owner] == generation) {
            record(trace_event{.kind = event_kind::timer_fired, .timer = id, .node = owner});
            cb();
        }
    });
    return id;
}

void network::schedule(std::chrono::nanoseconds time, std::function<void()> action) {
    events_.push(scheduled_event{time, next_seq_++, std::move(action)});
}

void network::deliver(const message& msg) {
    // Partitioned at delivery time: a message that was in flight when the
    // partition started is lost.
    if (partitioned_between(msg.from, msg.to)) {
        ++dropped_;
        record(trace_event{.kind = event_kind::dropped,
                           .from = msg.from,
                           .to = msg.to,
                           .bytes = msg.payload.size(),
                           .tag = msg.tag,
                           .reason = drop_reason::partition});
        return;
    }
    // Destination crashed at delivery time: a message that was in flight
    // when the node went down is lost. A message arriving after a restart
    // is delivered: the check is on the destination's state now, not on
    // when the message was sent.
    if (crashed_.contains(msg.to)) {
        ++dropped_;
        record(trace_event{.kind = event_kind::dropped,
                           .from = msg.from,
                           .to = msg.to,
                           .bytes = msg.payload.size(),
                           .tag = msg.tag,
                           .reason = drop_reason::crashed});
        return;
    }
    ++delivered_;
    record(trace_event{.kind = event_kind::delivered,
                       .from = msg.from,
                       .to = msg.to,
                       .bytes = msg.payload.size(),
                       .tag = msg.tag});
    if (nodes_[msg.to]->deliver(msg)) {
        return;
    }
    // No handler consumed the message. Strict mode throws, but only for
    // nodes that opted into typed dispatch: a node with no typed handlers
    // keeps the v0.1.0 behavior (silently ignore, still counted).
    if (!strict_tags_ || !nodes_[msg.to]->has_typed_handlers()) {
        return;
    }
    const node& dest = *nodes_[msg.to];
    std::string tag_description = "no type tag (payload too short: " +
                                  std::to_string(msg.payload.size()) + " bytes)";
    if (msg.payload.size() >= sizeof(std::uint64_t)) {
        tag_description = "tag " + std::to_string(detail::read_u64(msg.payload.data()));
    }
    throw unhandled_message("simulnetx: unhandled_message: node '" + dest.name() +
                            "' received a message with " + tag_description +
                            " that no on<T> handler matches and no on_message catch-all is "
                            "registered (seed " +
                            std::to_string(seed_) + ", t=" +
                            std::to_string(now_.count()) +
                            "ns). Register on<T> for the type, add an on_message catch-all, "
                            "or call network::set_strict_tags(false) to ignore unmatched "
                            "messages.");
}

bool network::partitioned_between(node_id a, node_id b) const {
    return (partition_a_.contains(a) && partition_b_.contains(b)) ||
           (partition_b_.contains(a) && partition_a_.contains(b));
}

std::chrono::nanoseconds network::sample_delay(std::chrono::nanoseconds range) {
    if (range.count() <= 0) {
        return std::chrono::nanoseconds{0};
    }
    const double fraction = rng_.next_double();
    const auto extra = static_cast<std::int64_t>(fraction * static_cast<double>(range.count()));
    return std::chrono::nanoseconds{extra};
}

}  // namespace simulnetx
