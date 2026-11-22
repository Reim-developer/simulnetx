#pragma once

#include <cstdint>

namespace simulnetx {

// splitmix64: a small, fast, fully specified PRNG.
//
// Every operation is integer arithmetic on std::uint64_t, so the output stream
// is identical on every platform and every standard library. The simulation
// draws randomness ONLY from this class - never from
// std::uniform_*_distribution, whose output the standard does not fix, so the
// same seed would produce different results under libstdc++ and libc++.
class splitmix64 {
public:
    explicit splitmix64(std::uint64_t seed) : state_(seed) {}

    // Next 64-bit value in the stream.
    std::uint64_t next_u64();

    // Uniform double in [0, 1): the top 53 bits of next_u64(), scaled by 2^-53.
    // Exact: the scale factor is a power of two and the value fits in the
    // 53-bit double mantissa, so this is bit-identical on all IEEE 754 doubles.
    double next_double();

private:
    std::uint64_t state_;
};

}  // namespace simulnetx
