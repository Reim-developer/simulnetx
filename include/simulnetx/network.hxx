#pragma once

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <queue>
#include <string>
#include <unordered_set>
#include <utility>
#include <vector>

#include "simulnetx/expectation.hxx"
#include "simulnetx/link_config.hxx"
#include "simulnetx/message.hxx"
#include "simulnetx/node.hxx"
#include "simulnetx/rng.hxx"
#include "simulnetx/trace.hxx"

namespace simulnetx {

// An in-process simulated network with a virtual clock.
//
// Same seed + same scenario = identical results on every run, on every
// platform: the event queue is ordered by (virtual time, sequence number) and
// all randomness comes from the hand-rolled splitmix64 RNG.
//
// The simulation is single-threaded and uses no real time, no real threads,
// no sockets and no OS calls.
class network {
public:
    explicit network(std::uint64_t seed) : seed_(seed), rng_(seed) {}

    network(const network&) = delete;
    network& operator=(const network&) = delete;
    network(network&&) = delete;
    network& operator=(network&&) = delete;

    // Add a node. The returned reference stays valid across later add_node
    // calls and for the lifetime of the network.
    node& add_node(std::string name);

    // Connect two nodes with the given behavior, in both directions. A later
    // link() or link_directed() call replaces the config of the direction(s)
    // it covers. Throws std::invalid_argument for unknown node ids, a
    // self-link, or an invalid config.
    void link(node_id a, node_id b, link_config cfg);

    // Connect two nodes in ONE direction only: from -> to uses cfg, while
    // to -> from is unchanged (and still has no link if none was set).
    // A later link_directed() call replaces only that direction's config; a
    // later link() call resets both directions. Throws
    // std::invalid_argument for unknown node ids, a self-link, or an invalid
    // config.
    void link_directed(node_id from, node_id to, link_config cfg);

    // Drop all messages between the two groups while the partition lasts
    // (counted as dropped). Nodes in neither group are unaffected, and
    // messages within a group still flow. A message sent while partitioned is
    // dropped at send time; a message in flight when the partition starts is
    // dropped at delivery time. Throws std::invalid_argument if a node id is
    // unknown or appears in both groups.
    void partition(std::vector<node_id> group_a, std::vector<node_id> group_b);

    // End the current partition, if any. Messages dropped while partitioned
    // stay dropped.
    void heal();

    // Crash a node: it stops processing. Messages TO it are dropped (checked
    // at send time and at delivery time, like a partition); it cannot send
    // or schedule timers (both throw std::logic_error); its pending timers
    // are cancelled and are NOT restored by restart. The message handler is
    // preserved across restart: it is code, not state. Crash consumes no
    // RNG draws, and is independent of partitions: restart does not heal a
    // partition. Crashing an already-crashed node is a no-op. Throws
    // std::invalid_argument for an unknown node id.
    void crash(node_id id);

    // Restart a crashed node: it can receive, send and schedule again. A
    // message whose delivery time falls after the restart IS delivered
    // (messages are external to the process, so they survive a crash if they
    // arrive after the restart). Restarting a live node is a no-op. Throws
    // std::invalid_argument for an unknown node id.
    void restart(node_id id);

    // Whether the node is currently crashed. Throws std::invalid_argument
    // for an unknown node id.
    bool is_crashed(node_id id) const;

    // Advance the virtual clock to t, processing all events scheduled at or
    // before t in (time, sequence) order. Throws std::invalid_argument if t
    // is before the current virtual time.
    void run_until(std::chrono::nanoseconds t);

    // Advance the virtual clock by d. Throws std::invalid_argument for a
    // negative duration.
    void run_for(std::chrono::nanoseconds d);

    // Schedule a callback at the current virtual time + delay. Network timers
    // are infrastructure, not node behavior: they are NOT cancelled by node
    // crashes, and they need no node to exist. Callbacks may send messages,
    // schedule further timers and crash/restart nodes. Returns the timer's
    // id (see trace.hxx); the id appears in the timer_fired trace event.
    // Throws std::invalid_argument for a negative delay.
    timer_id after(std::chrono::nanoseconds delay, std::function<void()> callback);

    // Schedule a callback at absolute virtual time t. Same crash immunity as
    // after(). Returns the timer's id. Throws std::invalid_argument if t is
    // before the current virtual time.
    timer_id schedule_at(std::chrono::nanoseconds t, std::function<void()> callback);

    // Current virtual time.
    std::chrono::nanoseconds now() const { return now_; }

    // Strict tag dispatch (default: true). When a message reaches a node
    // that has typed handlers but no handler matches it and no on_message
    // catch-all is registered, strict mode throws unhandled_message out of
    // run_until; lenient mode silently ignores the message (it still
    // counts as delivered). Nodes with no typed handlers are unaffected
    // either way. A runtime flag, deliberately: not a macro (TU-global,
    // fights the templates-not-macros rule) and not a template parameter
    // on network (it would infect every type in the API). Silent dropping
    // is the implicit behavior, so it is the opt-in.
    void set_strict_tags(bool strict) { strict_tags_ = strict; }
    bool strict_tags() const { return strict_tags_; }

    // Statistics as checkable expectations. Each expectation carries the seed
    // and the current virtual time, so a failed check reports both.
    expectation<std::size_t> sent_count() const;
    expectation<std::size_t> delivered_count() const;
    expectation<std::size_t> dropped_count() const;

    // The event trace: an ordered log of everything the simulation did
    // (see trace.hxx). A live view: the vector grows as the sim runs. The
    // reference stays valid; iterators may invalidate on growth (standard
    // vector caveat). Tracing is a pure observer: recording consumes no
    // RNG draws and never affects scheduling, counts, deliveries or drops.
    const std::vector<trace_event>& trace() const { return trace_; }

    // Drop all recorded events.
    void clear_trace() { trace_.clear(); }

    // Tracing is ON by default. set_tracing(false) stops recording
    // (already-recorded events are kept); re-enabling resumes.
    void set_tracing(bool enabled) { tracing_ = enabled; }
    bool tracing() const { return tracing_; }

private:
    friend class node;

    // One scheduled action: a message delivery or a timer callback. Ordered by
    // (time, sequence): the sequence number is a monotonically increasing
    // tiebreaker, so the order is fully deterministic.
    struct scheduled_event {
        std::chrono::nanoseconds time;
        std::uint64_t seq;
        std::function<void()> action;
    };

    // Later (time, sequence) sorts first in the priority queue, so the
    // earliest event is always on top.
    struct event_later {
        bool operator()(const scheduled_event& a, const scheduled_event& b) const {
            if (a.time != b.time) {
                return a.time > b.time;
            }
            return a.seq > b.seq;
        }
    };

    // Record one event, stamped with the current virtual time. A no-op
    // when tracing is disabled. Consumes no RNG draws.
    void record(trace_event event) {
        if (tracing_) {
            event.at = now_;
            trace_.push_back(event);
        }
    }

    void send_from(node_id from, node_id to, std::vector<std::byte> payload,
                   std::uint64_t tag = 0);
    // Schedule a timer owned by one node. The callback captures the node's
    // generation at schedule time and no-ops if the generation changed
    // (i.e. the node crashed in between), so node timers are cancelled by a
    // crash and never restored by a restart. Returns the timer's id.
    // Throws std::logic_error if the node is currently crashed,
    // std::invalid_argument for a negative delay.
    timer_id schedule_node_timer(node_id owner, std::chrono::nanoseconds delay,
                                 std::function<void()> callback);
    void schedule(std::chrono::nanoseconds time, std::function<void()> action);
    void deliver(const message& msg);

    bool partitioned_between(node_id a, node_id b) const;

    // Uniform delay in [0, range] sampled from the RNG. Returns 0 when the
    // range is 0 (and draws nothing from the RNG in that case).
    std::chrono::nanoseconds sample_delay(std::chrono::nanoseconds range);

    std::uint64_t seed_;
    splitmix64 rng_;
    std::chrono::nanoseconds now_{0};
    std::uint64_t next_seq_{0};

    std::vector<std::unique_ptr<node>> nodes_;
    std::map<std::pair<node_id, node_id>, link_config> links_;
    std::unordered_set<node_id> partition_a_;
    std::unordered_set<node_id> partition_b_;

    // Crashed nodes, and the per-node generation counter: crash() increments
    // a node's generation, which cancels its pending timers (they capture the
    // generation at schedule time and no-op on mismatch). Restart does not
    // reset the generation, so pre-crash timers never fire.
    std::unordered_set<node_id> crashed_;
    std::vector<std::uint64_t> generations_;

    std::priority_queue<scheduled_event, std::vector<scheduled_event>, event_later> events_;

    std::size_t sent_{0};
    std::size_t delivered_{0};
    std::size_t dropped_{0};

    // The event trace (on by default) and the timer-id counter. Timer ids
    // are unique across node and network timers: one counter, one choke
    // point.
    std::vector<trace_event> trace_;
    bool tracing_{true};
    timer_id next_timer_id_{0};

    // Strict tag dispatch; see set_strict_tags. Default true.
    bool strict_tags_{true};
};

}  // namespace simulnetx
