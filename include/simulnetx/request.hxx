#pragma once

// The request/response engine: node::request<Req, Resp> sends a typed
// request and returns a handle for configuring retries, a timeout, and
// the callbacks; node::serve<Req, Resp> registers a handler that replies
// to requests of one type.
//
// Wire format: requests and responses travel in internal detail envelopes
// ([kind tag][request_id][type tag][payload]) so a plain send<Resp> can
// never accidentally trigger a pending request. Request ids are per-node
// counters: an id is only ever looked up in the requesting node's own
// pending map, so per-node uniqueness is exactly what is needed (and a
// per-network counter would need node::request to reach into network,
// which is incomplete where node::request is defined).
//
// Semantics: the first request fires IMMEDIATELY at the request() call -
// safe because time only advances under run_until, so a response can never
// arrive before the caller attaches on_response. Retries reuse the same
// request id; the pending entry is removed on the first response, so a
// duplicate or late response is ignored (at-most-once delivery to the
// callback). A timeout removes the pending entry and calls on_timeout; a
// response arriving after the timeout is ignored. If the requesting node
// crashes, its pending requests are cancelled SILENTLY: the pending map is
// cleared and the retry/timeout timers (node timers) are cancelled by the
// crash, so a dead node's callbacks never fire. A server that never replies
// just lets the client time out - no error on the server side.
//
// A request_handle is tied to its network's lifetime, like a node&:
// calling retry_every/timeout_after after the network is destroyed is
// undefined behavior, because the arming lambdas capture the node.

#include <chrono>
#include <cstdint>
#include <functional>
#include <memory>
#include <stdexcept>

#include "simulnetx/message.hxx"
#include "simulnetx/serializer.hxx"

namespace simulnetx {

class node;

namespace detail {

// Shared mutable state of one in-flight request, owned jointly by the
// request_handle the caller holds and the node's pending map. The
// node-bound operations are captured lambdas set by node::request, so
// this header never needs network's definition.
struct request_state {
    std::uint64_t id = 0;
    node_id to = 0;
    std::chrono::nanoseconds retry_every{0};
    std::chrono::nanoseconds timeout_after{0};
    std::function<void()> on_timeout;
    // Is this request still awaiting a response? False once it completed,
    // timed out, or the node crashed (crash clears the pending map).
    std::function<bool()> is_pending;
    std::function<void()> erase_from_pending;
    // Re-send the request envelope with the same id and payload.
    std::function<void()> resend;
    // Schedule a node timer (cancelled by a crash, like any node timer).
    std::function<void(std::chrono::nanoseconds, std::function<void()>)> arm_timer;
    // Record a request_timeout trace event for this request.
    std::function<void()> record_timeout;
    // The self-re-arming retry callback, owned here so the re-arming
    // lambda can hold only weak references to it (no reference cycle).
    std::shared_ptr<std::function<void()>> retry_callback;
};

}  // namespace detail

// Handle for one in-flight request, returned by node::request. Configures
// retries, a timeout, and the callbacks; every method returns *this so
// the configuration chains:
//
//   node.request<hello, world>(server, hello{"reim"})
//       .retry_every(1s)
//       .timeout_after(10s)
//       .on_response([](const world& w) { ... })
//       .on_timeout([] { ... });
//
// retry_every/timeout_after are measured from when they are called - in
// the usual chained call, that is the request time, because no virtual
// time passes between request() and the chained calls.
template <serializable Req, serializable Resp>
class request_handle {
public:
    // Retry the request every d until a response arrives or the request
    // times out (or the node crashes). Retries reuse the same request id.
    // Throws std::logic_error if called twice on the same handle, and
    // std::invalid_argument for a non-positive interval.
    request_handle& retry_every(std::chrono::nanoseconds d) {
        if (state_->retry_every.count() != 0) {
            throw std::logic_error("simulnetx: retry_every() called twice on the same request");
        }
        if (d.count() <= 0) {
            throw std::invalid_argument("simulnetx: retry interval must be positive");
        }
        state_->retry_every = d;
        // The retry callback re-arms itself through weak pointers: the
        // state owns the callback, the callback only weakly refers to the
        // state and to itself, so no reference cycle keeps a completed
        // request alive.
        auto callback = std::make_shared<std::function<void()>>();
        std::weak_ptr<detail::request_state> weak_state = state_;
        std::weak_ptr<std::function<void()>> weak_self = callback;
        *callback = [weak_state, weak_self]() {
            const auto state = weak_state.lock();
            if (state == nullptr || !state->is_pending()) {
                return;  // Response arrived, timed out, or the node crashed.
            }
            state->resend();
            if (const auto self = weak_self.lock()) {
                state->arm_timer(state->retry_every, *self);
            }
        };
        state_->retry_callback = callback;
        state_->arm_timer(d, *callback);
        return *this;
    }

    // Give up after d with no response: the pending entry is removed and
    // on_timeout is called. A response arriving after the timeout is
    // ignored. Throws std::logic_error if called twice on the same handle,
    // and std::invalid_argument for a non-positive timeout.
    request_handle& timeout_after(std::chrono::nanoseconds d) {
        if (state_->timeout_after.count() != 0) {
            throw std::logic_error("simulnetx: timeout_after() called twice on the same request");
        }
        if (d.count() <= 0) {
            throw std::invalid_argument("simulnetx: timeout must be positive");
        }
        state_->timeout_after = d;
        state_->arm_timer(d, [state = state_]() {
            if (!state->is_pending()) {
                return;  // Response arrived, or the node crashed.
            }
            state->erase_from_pending();
            if (state->record_timeout) {
                state->record_timeout();
            }
            if (state->on_timeout) {
                state->on_timeout();
            }
        });
        return *this;
    }

    // Set the callback for the first response. Replaces any previously set
    // callback. A request with no on_response still completes (the pending
    // entry is removed on the first response); the response is just not
    // observed.
    request_handle& on_response(std::function<void(const Resp&)> f) {
        *on_response_ = std::move(f);
        return *this;
    }

    // Set the callback for the timeout. Replaces any previously set
    // callback. Only fires if timeout_after was also configured.
    request_handle& on_timeout(std::function<void()> f) {
        state_->on_timeout = std::move(f);
        return *this;
    }

private:
    friend class node;

    request_handle(std::shared_ptr<detail::request_state> state,
                   std::shared_ptr<std::function<void(const Resp&)>> on_response)
        : state_(std::move(state)), on_response_(std::move(on_response)) {}

    std::shared_ptr<detail::request_state> state_;
    std::shared_ptr<std::function<void(const Resp&)>> on_response_;
};

}  // namespace simulnetx
