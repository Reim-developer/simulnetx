#include "simulnetx/network.hxx"

#include <algorithm>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <utility>

#include "simulnetx/errors.hxx"
#include "simulnetx/json.hxx"
#include "simulnetx/serializer.hxx"
#include "simulnetx/tcp.hxx"
#include "simulnetx/udp.hxx"

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

// Add two nanosecond durations, throwing instead of overflowing: virtual
// times near int64 max (292 years) are absurd, but a wrap would silently
// deliver a message in the past, so the arithmetic is checked wherever a
// bandwidth finish time can push a sum toward the limit.
std::chrono::nanoseconds checked_add(std::chrono::nanoseconds a, std::chrono::nanoseconds b) {
    if (b.count() > 0 &&
        a.count() > std::numeric_limits<std::int64_t>::max() - b.count()) {
        throw std::invalid_argument(
            "simulnetx: virtual time overflows int64 nanoseconds");
    }
    return a + b;
}

// floor(bytes * 8 * 1e9 / bps) nanoseconds of serialization delay: how
// long the link is busy transmitting a message of this size.
// Division-first, so the product bytes * 8 * 1e9 is never formed whole
// (it overflows u64 past ~2.3 GB payloads, and there is no __int128 on
// MSVC):
//   D = (bytes / bps) * 8e9 + ((bytes % bps) * 8e9) / bps
// and the remainder term splits 8e9 = 8 * 1e9, then 1e9 = 1e3 * 1e6, so
// every intermediate multiply stays in u64 for bandwidths up to ~1.8e13
// Bps (18 TB/s); past that, or past int64 nanoseconds of delay, the
// guards below throw rather than overflowing silently.
std::chrono::nanoseconds serialization_delay(std::size_t bytes, std::uint64_t bps) {
    const std::uint64_t count = static_cast<std::uint64_t>(bytes);
    const std::uint64_t whole = count / bps;
    const std::uint64_t rem = count % bps;
    // Whole part: whole * 8e9 must fit in int64 ns, else the delay is
    // unrepresentable as a duration.
    if (whole >
        static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max()) / 8'000'000'000ULL) {
        throw std::invalid_argument(
            "simulnetx: serialization delay overflows int64 nanoseconds");
    }
    std::uint64_t delay = whole * 8'000'000'000ULL;
    // Remainder: floor(rem * 8e9 / bps). rem < bps and rem <= bytes, a
    // real payload size, so rem * 8 cannot overflow.
    const std::uint64_t rem8 = rem * 8;
    delay += (rem8 / bps) * 1'000'000'000ULL;
    const std::uint64_t x = rem8 % bps;  // < bps
    // floor(x * 1e9 / bps), split 1e9 = 1e3 * 1e6. x * 1e3 is guarded:
    // it overflows only past ~1.8e16 Bps.
    if (x > std::numeric_limits<std::uint64_t>::max() / 1'000ULL) {
        throw std::invalid_argument("simulnetx: serialization delay overflows for "
                                    "bandwidths above ~1.8e16 Bps");
    }
    const std::uint64_t y = x * 1'000ULL;
    delay += (y / bps) * 1'000'000ULL;
    const std::uint64_t z = y % bps;  // < bps
    // z * 1e6: guarded the same way (overflows only past ~1.8e13 Bps
    // with a large remainder).
    if (z > std::numeric_limits<std::uint64_t>::max() / 1'000'000ULL) {
        throw std::invalid_argument("simulnetx: serialization delay overflows for "
                                    "bandwidths above ~1.8e13 Bps");
    }
    delay += (z * 1'000'000ULL) / bps;
    if (delay > static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max())) {
        throw std::invalid_argument(
            "simulnetx: serialization delay overflows int64 nanoseconds");
    }
    return std::chrono::nanoseconds{static_cast<std::int64_t>(delay)};
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
    // Sockets on the crashed node lose their in-flight state: pending
    // receives are cancelled and queued datagrams dropped. Bindings
    // survive (see udp.hxx). The notification schedules zero-delay
    // network events, so it never mutates the registry mid-iteration.
    for (const auto& [key, sock] : sockets_) {
        if (key.first == id) {
            sock->on_node_crashed();
        }
    }
    // TCP connections on the crashed node lose their connection state:
    // pending ops are cancelled, reassembly buffers dropped. Bindings
    // survive (see tcp.hxx). Snapshot the states first: on_node_crashed
    // unregisters the connection, which would mutate the map.
    std::vector<std::shared_ptr<tcp::detail::tcp_connection>> doomed;
    for (const auto& [key, conn] : tcp_connections_) {
        if (std::get<0>(key) == id) {
            doomed.push_back(conn);
        }
    }
    for (const auto& conn : doomed) {
        conn->on_node_crashed();
    }
    // TCP listeners on the crashed node drop their queued connections;
    // the listener binding survives, so the acceptor works again after
    // the restart.
    std::vector<tcp::acceptor*> dead_listeners;
    for (const auto& [key, acc] : tcp_listeners_) {
        if (key.first == id) {
            dead_listeners.push_back(acc);
        }
    }
    for (tcp::acceptor* acc : dead_listeners) {
        acc->on_node_crashed();
    }
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

expectation<std::size_t> network::event_count(event_kind kind) const {
    std::size_t count = 0;
    for (const trace_event& event : trace_) {
        if (event.kind == kind) {
            ++count;
        }
    }
    return expectation<std::size_t>(
        count, std::string("event_count(") + event_kind_name(kind) + ")",
        detail::sim_context{seed_, now_});
}

std::string network::node_name(node_id id) const {
    if (id < nodes_.size()) {
        return nodes_[id]->name();
    }
    return "node#" + std::to_string(id);
}

void network::write_event(std::ostream& out, const trace_event& event) const {
    out << "t=" << event.at.count() << "ns ";
    switch (event.kind) {
        case event_kind::sent:
        case event_kind::delivered:
        case event_kind::duplicated:
        case event_kind::reordered:
            out << node_name(event.from) << " -> " << node_name(event.to) << " "
                << event_kind_name(event.kind) << " " << event.bytes << " bytes";
            if (event.tag != 0) {
                out << " [tag=" << event.tag << "]";
            }
            break;
        case event_kind::dropped:
            out << node_name(event.from) << " -> " << node_name(event.to) << " dropped ("
                << drop_reason_name(event.reason) << ") " << event.bytes << " bytes";
            if (event.tag != 0) {
                out << " [tag=" << event.tag << "]";
            }
            break;
        case event_kind::timer_fired:
            out << "timer #" << event.timer << " fired ";
            if (event.node == no_node) {
                out << "[network]";
            } else {
                out << "[node " << node_name(event.node) << "]";
            }
            break;
        case event_kind::node_crashed:
            out << "node " << node_name(event.node) << " crashed";
            break;
        case event_kind::node_restarted:
            out << "node " << node_name(event.node) << " restarted";
            break;
        case event_kind::partitioned:
            out << "partition started";
            break;
        case event_kind::healed:
            out << "partition healed";
            break;
        case event_kind::request_sent:
            out << node_name(event.from) << " -> " << node_name(event.to) << " request #"
                << event.request_id << " sent (attempt " << event.attempt << ")";
            break;
        case event_kind::response_received:
            out << node_name(event.from) << " -> " << node_name(event.to) << " response #"
                << event.request_id << " received";
            break;
        case event_kind::request_timeout:
            out << "node " << node_name(event.node) << " request #" << event.request_id
                << " timed out";
            break;
    }
}

std::string network::dump_trace() const {
    std::ostringstream oss;
    dump_trace(oss);
    return oss.str();
}

void network::dump_trace(std::ostream& out) const {
    // '\n', never std::endl: endl flushes per line, which would kill
    // throughput on a big trace. The caller flushes when they want to.
    out << "trace: " << trace_.size() << " events (seed " << seed_ << ")\n";
    for (const trace_event& event : trace_) {
        write_event(out, event);
        out << '\n';
    }
}

std::string network::format_event(const trace_event& event) const {
    std::ostringstream oss;
    write_event(oss, event);
    return oss.str();
}

std::string network::dump_trace_json() const {
    std::ostringstream oss;
    dump_trace_json(oss);
    return oss.str();
}

void network::dump_trace_json(std::ostream& out) const {
    json::array events;
    events.reserve(trace_.size());
    for (const trace_event& event : trace_) {
        events.push_back(json::to_json(event));
    }
    const json::value root{json::object{
        {"seed", json::value{seed_}},
        {"events", json::value{std::move(events)}},
    }};
    json::dump(root, out);
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

    // Bandwidth: the link is busy serializing each message in turn
    // (strict FIFO per directed link). Unlimited (0) means no delay and
    // no queueing. The queueing consumes no RNG, and free_at advances at
    // send time: a message dropped later by the delivery check (a
    // partition landing mid-transmission, a crash) still consumed its
    // transmission. A duplicated copy is a second transmission: it
    // consumes bandwidth and queues behind the original, and still
    // arrives 1ns after the first.
    std::chrono::nanoseconds finish = now_;
    std::chrono::nanoseconds copy_finish = now_;
    if (cfg.bandwidth_bps > 0) {
        finish = transmit_at(from, to, bytes, cfg.bandwidth_bps);
        if (duplicate) {
            copy_finish = transmit_at(from, to, bytes, cfg.bandwidth_bps);
        }
    }
    const std::chrono::nanoseconds arrival =
        checked_add(checked_add(finish, cfg.latency), extra_delay);
    message msg{from, to, std::move(payload)};
    msg.tag = tag;
    if (duplicate) {
        // The second copy is identical and arrives 1ns after the first.
        // (Built before msg moves into the delivery lambda below.)
        message copy{from, to, msg.payload};
        copy.tag = tag;
        const std::chrono::nanoseconds copy_arrival =
            checked_add(checked_add(checked_add(copy_finish, cfg.latency), extra_delay),
                        std::chrono::nanoseconds{1});
        schedule(copy_arrival, [this, copy = std::move(copy)] { deliver(copy); });
    }
    schedule(arrival, [this, msg = std::move(msg)] { deliver(msg); });
}

std::chrono::nanoseconds network::transmit_at(node_id from, node_id to, std::size_t bytes,
                                              std::uint64_t bandwidth_bps) {
    std::chrono::nanoseconds& free_at = link_free_at_[{from, to}];
    const std::chrono::nanoseconds start = std::max(now_, free_at);
    const std::chrono::nanoseconds finish =
        checked_add(start, serialization_delay(bytes, bandwidth_bps));
    free_at = finish;
    return finish;
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

void network::schedule_silent(std::function<void()> action) {
    schedule(now_, std::move(action));
}

void network::schedule_silent_at(std::chrono::nanoseconds t,
                                 std::function<void()> action) {
    if (t < now_) {
        throw std::invalid_argument(
            "simulnetx: schedule_silent_at time is before the current virtual time");
    }
    schedule(t, std::move(action));
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

std::uint16_t network::register_socket(node_id id, std::uint16_t port,
                                       udp::socket* sock) {
    std::uint16_t actual = port;
    if (actual == 0) {
        actual = allocate_ephemeral_port(id);
    }
    const auto key = std::make_pair(id, actual);
    if (sockets_.contains(key)) {
        throw std::logic_error("simulnetx: port " + std::to_string(actual) +
                               " is already bound on node " + node_name(id));
    }
    sockets_.emplace(key, sock);
    return actual;
}

void network::unregister_socket(node_id id, std::uint16_t port) {
    sockets_.erase(std::make_pair(id, port));
}

void network::deliver_datagram(node_id to, std::uint16_t dst_port, node_id from,
                               std::uint16_t src_port, std::vector<std::byte> payload) {
    const auto it = sockets_.find(std::make_pair(to, dst_port));
    if (it == sockets_.end()) {
        return;  // No socket bound to this port: silent drop (see udp.hxx).
    }
    it->second->on_datagram(udp::endpoint{from, src_port}, std::move(payload));
}

std::uint16_t network::allocate_ephemeral_port(node_id id) {
    std::uint16_t& next = next_ephemeral_[id];
    if (next == 0) {
        next = 49152;  // First allocation for this node; the counter never
                       // returns to 0 afterwards (it wraps 65535 -> 49152).
    }
    for (int i = 0; i < 16384; ++i) {
        const std::uint16_t candidate = next;
        next = static_cast<std::uint16_t>(next == 65535 ? 49152 : next + 1);
        if (!sockets_.contains(std::make_pair(id, candidate))) {
            return candidate;
        }
    }
    throw std::logic_error("simulnetx: no ephemeral ports left on node " +
                           node_name(id));
}

std::uint16_t network::claim_tcp_port(node_id id, std::uint16_t port) {
    std::uint16_t actual = port;
    if (actual == 0) {
        actual = allocate_tcp_ephemeral_port(id);
    }
    const auto key = std::make_pair(id, actual);
    if (tcp_bound_ports_.contains(key)) {
        throw std::logic_error("simulnetx: TCP port " + std::to_string(actual) +
                               " is already bound on node " + node_name(id));
    }
    tcp_bound_ports_.insert(key);
    return actual;
}

void network::release_tcp_port(node_id id, std::uint16_t port) {
    tcp_bound_ports_.erase(std::make_pair(id, port));
}

std::uint16_t network::allocate_tcp_ephemeral_port(node_id id) {
    std::uint16_t& next = next_tcp_ephemeral_[id];
    if (next == 0) {
        next = 49152;  // First allocation for this node; the counter never
                       // returns to 0 afterwards (it wraps 65535 -> 49152).
    }
    for (int i = 0; i < 16384; ++i) {
        const std::uint16_t candidate = next;
        next = static_cast<std::uint16_t>(next == 65535 ? 49152 : next + 1);
        if (!tcp_bound_ports_.contains(std::make_pair(id, candidate))) {
            return candidate;
        }
    }
    throw std::logic_error("simulnetx: no TCP ephemeral ports left on node " +
                           node_name(id));
}

void network::register_tcp_listener(node_id id, std::uint16_t port,
                                    tcp::acceptor* acc) {
    tcp_listeners_.emplace(std::make_pair(id, port), acc);
}

void network::unregister_tcp_listener(node_id id, std::uint16_t port) {
    tcp_listeners_.erase(std::make_pair(id, port));
}

void network::register_tcp_connection(
    const tcp_key& key, std::shared_ptr<tcp::detail::tcp_connection> conn) {
    if (tcp_connections_.contains(key)) {
        throw std::logic_error(
            "simulnetx: a TCP connection for this address pair already exists");
    }
    tcp_connections_.emplace(key, std::move(conn));
}

void network::unregister_tcp_connection(const tcp_key& key) {
    tcp_connections_.erase(key);
}

void network::deliver_segment(node_id to, std::uint16_t dst_port, node_id from,
                              std::uint16_t src_port, std::uint32_t flags,
                              std::uint64_t seq, std::uint64_t ack,
                              std::vector<std::byte> payload) {
    const tcp_key key{to, dst_port, from, src_port};
    if (const auto it = tcp_connections_.find(key); it != tcp_connections_.end()) {
        it->second->on_segment(from, src_port, flags, seq, ack, std::move(payload));
        return;
    }
    const bool is_rst = (flags & tcp::detail::tcp_flag_rst) != 0;
    if (is_rst) {
        return;  // RST for a nonexistent connection: drop silently.
    }
    const bool is_syn = (flags & tcp::detail::tcp_flag_syn) != 0;
    const bool is_ack = (flags & tcp::detail::tcp_flag_ack) != 0;
    if (is_syn && !is_ack) {
        if (const auto lit = tcp_listeners_.find(std::make_pair(to, dst_port));
            lit != tcp_listeners_.end()) {
            lit->second->on_syn(from, src_port, seq);
            return;
        }
        // No listener on this port: the connection is refused.
    }
    // Any other segment for a nonexistent connection: the peer is gone,
    // so answer RST (best-effort: a missing return link is swallowed;
    // the sender's RTO reports timed_out instead).
    try {
        nodes_[to]->send_segment(from, dst_port, src_port, tcp::detail::tcp_flag_rst, 0, 0,
                                 {});
    } catch (...) {
    }
}

}  // namespace simulnetx
