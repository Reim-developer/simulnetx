#pragma once

#include <chrono>
#include <cstdint>

namespace simulnetx {

// Per-link behavior for ONE direction of travel.
//
// latency: base one-way delay added to every delivery.
// jitter: an extra uniform [0, jitter] delay sampled per message.
// loss_rate: probability a message is dropped (counted as dropped).
// dup_rate: probability a second identical copy is delivered 1ns after the
//   first (counted as an extra delivery).
// reorder_rate: probability a message gets an extra uniform [0, jitter] delay,
//   so it may arrive after a later-sent message. Has no visible effect when
//   jitter is zero.
//
// Rates must be in [0, 1] and delays must be non-negative;
// network::link and network::link_directed throw std::invalid_argument
// otherwise. network::link applies one config to both directions;
// network::link_directed applies it to a single direction.
struct link_config {
    std::chrono::nanoseconds latency{0};
    std::chrono::nanoseconds jitter{0};
    double loss_rate{0.0};
    double dup_rate{0.0};
    double reorder_rate{0.0};
    // Bytes per second the link can carry; 0 = unlimited (the default,
    // backward compatible). A message of N bytes needs
    // floor(N * 8 * 1e9 / bandwidth_bps) ns on the wire, and messages
    // queue FIFO behind each other on the same directed link: the second
    // of two back-to-back messages starts only when the first finishes.
    // Delivery time is finish + latency (+ the jitter sample, + the
    // reorder extra delay, exactly as without bandwidth). A duplicated
    // copy is a second transmission: it consumes bandwidth and queues
    // behind the original.
    std::uint64_t bandwidth_bps{0};
};

}  // namespace simulnetx
