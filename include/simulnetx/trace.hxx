#pragma once

// Event tracing (v0.5.0 design, spike implementation): an ordered log of
// everything the simulation did, with virtual timestamps.
//
// The trace is DATA, consumed by filters, dumps and equality checks —
// hence a flat struct, not a variant. Fields are meaningful only for
// their kinds (see the table on trace_event); the record sites fill
// exactly their kind's fields.

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <limits>

#include "simulnetx/message.hxx"

namespace simulnetx {

// Timer identity. Assigned per network, from 0, in schedule order.
// Returned by network::after / schedule_at and node::after.
using timer_id = std::uint64_t;

// Owner field for events with no node (network timers).
inline constexpr node_id no_node = std::numeric_limits<node_id>::max();

enum class event_kind {
    sent,
    delivered,
    dropped,
    duplicated,  // a dup copy was scheduled (its delivery is a plain delivered)
    reordered,   // extra delay was applied at send time
    timer_fired,
    node_crashed,
    node_restarted,
    partitioned,
    healed,
    request_sent,
    response_received,
    request_timeout,
};

enum class drop_reason { loss, partition, crashed };

// The stable name of an event kind, as printed by dump_trace() and
// dump_trace_json(). Frozen in 1.0.0.
inline const char* event_kind_name(event_kind kind) {
    switch (kind) {
        case event_kind::sent:
            return "sent";
        case event_kind::delivered:
            return "delivered";
        case event_kind::dropped:
            return "dropped";
        case event_kind::duplicated:
            return "duplicated";
        case event_kind::reordered:
            return "reordered";
        case event_kind::timer_fired:
            return "timer_fired";
        case event_kind::node_crashed:
            return "node_crashed";
        case event_kind::node_restarted:
            return "node_restarted";
        case event_kind::partitioned:
            return "partitioned";
        case event_kind::healed:
            return "healed";
        case event_kind::request_sent:
            return "request_sent";
        case event_kind::response_received:
            return "response_received";
        case event_kind::request_timeout:
            return "request_timeout";
    }
    return "unknown";
}

// The stable name of a drop reason, as printed by dump_trace() and
// dump_trace_json(). Frozen in 1.0.0.
inline const char* drop_reason_name(drop_reason reason) {
    switch (reason) {
        case drop_reason::loss:
            return "loss";
        case drop_reason::partition:
            return "partition";
        case drop_reason::crashed:
            return "crashed";
    }
    return "unknown";
}

struct trace_event {
    std::chrono::nanoseconds at{};  // virtual time of the event
    event_kind kind{};
    node_id from{};              // message events + request_sent/response_received
    node_id to{};                // message events + request_sent/response_received
    std::size_t bytes{};         // message events: payload size
    std::uint64_t tag{};         // message events: type tag the sender
                                 // declared, 0 for raw sends
    drop_reason reason{};        // dropped only
    timer_id timer{};            // timer_fired only
    node_id node{};              // timer_fired (owner, no_node for network
                                 // timers), crashed/restarted, request_timeout
    std::uint64_t request_id{};  // request_* only
    unsigned attempt{};          // request_sent only (1 = first fire)

    friend bool operator==(const trace_event&, const trace_event&) = default;
};

}  // namespace simulnetx
