#pragma once

#include <stdexcept>
#include <string>

namespace simulnetx {

// Thrown by expectation<T> checks on mismatch. The message always carries
// expected vs actual; expectations built by network also carry the seed and
// the current virtual time, so a failure is directly reproducible.
class assertion_failed : public std::runtime_error {
public:
    explicit assertion_failed(const std::string& message) : std::runtime_error(message) {}
};

// Thrown when decoding a typed message fails: tag mismatch, truncation, or
// malformed data (bad optional flag, variant index out of range, trailing
// bytes). Loud, never garbage: a decode either reproduces the sent value
// exactly or throws.
class decode_error : public std::runtime_error {
public:
    explicit decode_error(const std::string& message) : std::runtime_error(message) {}
};

}  // namespace simulnetx
