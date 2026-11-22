#pragma once

#include <chrono>
#include <cstdint>
#include <optional>
#include <sstream>
#include <string>
#include <utility>

#include "simulnetx/errors.hxx"

namespace simulnetx {

class network;

namespace detail {

// Simulation context attached to an expectation built by network: the seed
// and the virtual time at which the expectation was created. It is what makes
// a failure message reproducible.
struct sim_context {
    std::uint64_t seed;
    std::chrono::nanoseconds now;
};

template <typename T>
std::string stringify(const T& value) {
    std::ostringstream out;
    out << value;
    return out.str();
}

}  // namespace detail

// A checkable wrapper around a value, typically a simulation statistic
// returned by network (sent_count, delivered_count, dropped_count).
//
// Each check either passes silently or throws assertion_failed. The failure
// message carries expected vs actual plus, for expectations built by network,
// the seed and the current virtual time, e.g.:
//   expected delivered_count == 1, got 0 (seed 42, t=10500000000ns)
//
// The wrapper converts implicitly to T, so it is usable as a plain value:
//   std::size_t n = net.delivered_count();
template <typename T>
class expectation {
public:
    operator T() const { return value_; }

    const expectation& equal(const T& expected) const {
        if (!(value_ == expected)) {
            fail("==", detail::stringify(expected));
        }
        return *this;
    }

    const expectation& not_equal(const T& expected) const {
        if (!(value_ != expected)) {
            fail("!=", detail::stringify(expected));
        }
        return *this;
    }

    const expectation& greater_than(const T& expected) const {
        if (!(value_ > expected)) {
            fail(">", detail::stringify(expected));
        }
        return *this;
    }

    const expectation& less_than(const T& expected) const {
        if (!(value_ < expected)) {
            fail("<", detail::stringify(expected));
        }
        return *this;
    }

    const expectation& greater_or_equal(const T& expected) const {
        if (!(value_ >= expected)) {
            fail(">=", detail::stringify(expected));
        }
        return *this;
    }

    const expectation& less_or_equal(const T& expected) const {
        if (!(value_ <= expected)) {
            fail("<=", detail::stringify(expected));
        }
        return *this;
    }

    const expectation& between(const T& lo, const T& hi) const {
        if (value_ < lo || value_ > hi) {
            fail("in", "[" + detail::stringify(lo) + ", " + detail::stringify(hi) + "]");
        }
        return *this;
    }

private:
    friend class network;
    template <typename U>
    friend expectation<U> expect(U value);

    expectation(T value, std::string name, std::optional<detail::sim_context> context)
        : value_(value), name_(std::move(name)), context_(context) {}

    [[noreturn]] void fail(const std::string& condition, const std::string& expected_text) const {
        std::ostringstream out;
        out << "expected " << name_ << ' ' << condition << ' ' << expected_text << ", got "
            << detail::stringify(value_);
        if (context_.has_value()) {
            out << " (seed " << context_->seed << ", t=" << context_->now.count() << "ns)";
        }
        throw assertion_failed(out.str());
    }

    T value_;
    std::string name_;
    std::optional<detail::sim_context> context_;
};

// Wrap any value for fluent checks. No simulation context is attached, so a
// failure message carries expected vs actual only.
template <typename T>
expectation<T> expect(T value) {
    return expectation<T>(value, "value", std::nullopt);
}

}  // namespace simulnetx
