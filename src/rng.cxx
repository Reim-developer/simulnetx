#include "simulnetx/rng.hxx"

namespace simulnetx {

std::uint64_t splitmix64::next_u64() {
    std::uint64_t z = (state_ += 0x9e3779b97f4a7c15ULL);
    z = (z ^ (z >> 30)) * 0xbf58476d1ce4e5b9ULL;
    z = (z ^ (z >> 27)) * 0x94d049bb133111ebULL;
    return z ^ (z >> 31);
}

double splitmix64::next_double() {
    // Top 53 bits / 2^53.
    return static_cast<double>(next_u64() >> 11) * (1.0 / 9007199254740992.0);
}

}  // namespace simulnetx
