#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

namespace simulnetx {

// Small integer identifying a node. Assigned by network::add_node, starting
// at 0, in creation order.
using node_id = std::uint32_t;

// A message in flight between two nodes. The payload is raw bytes; nodes
// interpret it.
struct message {
    node_id from;
    node_id to;
    std::vector<std::byte> payload;
};

}  // namespace simulnetx
