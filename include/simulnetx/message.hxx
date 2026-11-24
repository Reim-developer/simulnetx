#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "simulnetx/serializer.hxx"

namespace simulnetx {

// Small integer identifying a node. Assigned by network::add_node, starting
// at 0, in creation order.
using node_id = std::uint32_t;

// A message in flight between two nodes. The payload is raw bytes; nodes
// interpret it.
//
// Typed messages (sent with node::send<T>) carry an 8-byte type tag first,
// then the serialized value. as<T>() checks the tag and decodes; a raw
// message has no tag, so as<T>() on one throws decode_error.
//
// tag is transport metadata, set by the sender at send time: the type tag
// the sender declared (0 for raw sends). It mirrors the first 8 payload
// bytes for typed messages but is recorded separately so the network never
// has to parse payloads (e.g. for trace events at delivery time).
struct message {
    node_id from;
    node_id to;
    std::vector<std::byte> payload;
    std::uint64_t tag{0};

    // Decode this message as a value of type T. Throws decode_error on a
    // tag mismatch (the message was sent as a different type, or is a raw
    // message with no tag), on truncation, and on trailing bytes after the
    // value. Loud, never garbage: either the sent value is reproduced
    // exactly or this throws.
    template <serializable T>
    T as() const {
        if (payload.size() < sizeof(std::uint64_t)) {
            throw decode_error("simulnetx: decode_error: message too short for a type tag (" +
                               std::to_string(payload.size()) + " bytes)");
        }
        const std::uint64_t actual = detail::read_u64(payload.data());
        const std::uint64_t expected = detail::type_tag<T>();
        if (actual != expected) {
            throw decode_error("simulnetx: decode_error: type tag mismatch (expected " +
                               std::to_string(expected) + ", got " + std::to_string(actual) +
                               "); the message was sent as a different type");
        }
        detail::cursor in(payload.data() + sizeof(std::uint64_t),
                          payload.size() - sizeof(std::uint64_t));
        T value = serializer<T>::decode(in);
        if (in.remaining() != 0) {
            throw decode_error("simulnetx: decode_error: trailing bytes after the value (" +
                               std::to_string(in.remaining()) + " bytes)");
        }
        return value;
    }
};

}  // namespace simulnetx
