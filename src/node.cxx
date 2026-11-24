#include "simulnetx/node.hxx"

#include <string>
#include <utility>

#include "simulnetx/errors.hxx"
#include "simulnetx/network.hxx"
#include "simulnetx/serializer.hxx"

namespace simulnetx {

void node::on_message(std::function<void(const message&)> handler) {
    handler_ = std::move(handler);
}

void node::send(node_id to, std::vector<std::byte> payload) {
    send_with_tag(to, std::move(payload), 0);
}

void node::send_with_tag(node_id to, std::vector<std::byte> payload, std::uint64_t tag) {
    owner_->send_from(id_, to, std::move(payload), tag);
}

void node::send(node_id to, std::string_view payload) {
    std::vector<std::byte> bytes;
    bytes.reserve(payload.size());
    for (const char c : payload) {
        bytes.push_back(static_cast<std::byte>(c));
    }
    send(to, std::move(bytes));
}

timer_id node::after(std::chrono::nanoseconds delay, std::function<void()> callback) {
    return owner_->schedule_node_timer(id_, delay, std::move(callback));
}

bool node::deliver(const message& msg) {
    if (msg.payload.size() >= sizeof(std::uint64_t)) {
        const std::uint64_t tag = detail::read_u64(msg.payload.data());
        // Envelopes first: a request/response envelope is routed by its
        // kind tag, never by the typed-handler map (a type tag colliding
        // with an envelope tag would misroute, probability 2^-64 - the
        // same accepted cost as any other tag collision).
        if (tag == detail::request_envelope_tag) {
            dispatch_request(msg);
            return true;
        }
        if (tag == detail::response_envelope_tag) {
            dispatch_response(msg);
            return true;
        }
        const auto it = typed_handlers_.find(tag);
        if (it != typed_handlers_.end()) {
            it->second(msg);
            return true;
        }
    }
    if (handler_) {
        handler_(msg);
        return true;
    }
    return false;
}

void node::dispatch_request(const message& msg) {
    detail::cursor in(msg.payload.data(), msg.payload.size());
    (void)in.take_u64();  // request_envelope_tag, already matched by deliver
    const std::uint64_t id = in.take_u64();
    const std::uint64_t req_tag = in.take_u64();
    const auto it = serve_handlers_.find(req_tag);
    if (it == serve_handlers_.end()) {
        return;  // No server for this request type: the client times out.
    }
    it->second(msg, id, in);
}

void node::dispatch_response(const message& msg) {
    detail::cursor in(msg.payload.data(), msg.payload.size());
    (void)in.take_u64();  // response_envelope_tag, already matched by deliver
    const std::uint64_t id = in.take_u64();
    const std::uint64_t resp_tag = in.take_u64();
    const auto it = pending_.find(id);
    if (it == pending_.end()) {
        return;  // Late or duplicate response: already completed or timed out.
    }
    // Erase before delivering: the first response completes the request,
    // so a duplicate arriving later finds no entry.
    pending_request entry = std::move(it->second);
    pending_.erase(it);
    if (resp_tag != entry.resp_tag) {
        throw decode_error("simulnetx: decode_error: response tag mismatch for request " +
                           std::to_string(id) + " (expected " +
                           std::to_string(entry.resp_tag) + ", got " +
                           std::to_string(resp_tag) +
                           "); the server replied with a different type than requested");
    }
    record_response_received(msg.from, id);
    entry.deliver(in);
}

void node::send_request_envelope(std::uint64_t id, node_id to, std::uint64_t req_tag,
                                 const std::vector<std::byte>& req_bytes) {
    std::vector<std::byte> payload;
    detail::append_u64(payload, detail::request_envelope_tag);
    detail::append_u64(payload, id);
    detail::append_u64(payload, req_tag);
    detail::append_bytes(payload, req_bytes.data(), req_bytes.size());
    send_with_tag(to, std::move(payload), req_tag);
}

void node::record_request_sent(std::uint64_t id, node_id to, unsigned attempt) {
    owner_->record(trace_event{.kind = event_kind::request_sent,
                               .from = id_,
                               .to = to,
                               .request_id = id,
                               .attempt = attempt});
}

void node::record_response_received(node_id from, std::uint64_t id) {
    owner_->record(trace_event{.kind = event_kind::response_received,
                               .from = from,
                               .to = id_,
                               .request_id = id});
}

void node::record_request_timeout(std::uint64_t id) {
    owner_->record(trace_event{.kind = event_kind::request_timeout,
                               .node = id_,
                               .request_id = id});
}

void node::cancel_pending_requests() {
    pending_.clear();
}

}  // namespace simulnetx
