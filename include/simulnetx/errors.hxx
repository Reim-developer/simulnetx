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

}  // namespace simulnetx
