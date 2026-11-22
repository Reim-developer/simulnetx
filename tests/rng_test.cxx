#include <cstdint>
#include <vector>

#include <gtest/gtest.h>

#include "simulnetx/rng.hxx"

namespace {

TEST(RngTest, KnownFirstValueForSeedZero) {
    // splitmix64 test vector: seed 0 produces 0xE220A8397B1DCDAF first.
    simulnetx::splitmix64 rng(0);
    EXPECT_EQ(rng.next_u64(), 0xE220A8397B1DCDAFULL);
}

TEST(RngTest, SameSeedProducesSameStream) {
    simulnetx::splitmix64 a(12345);
    simulnetx::splitmix64 b(12345);
    for (int i = 0; i < 1000; ++i) {
        EXPECT_EQ(a.next_u64(), b.next_u64());
    }
}

TEST(RngTest, DifferentSeedsProduceDifferentStreams) {
    simulnetx::splitmix64 a(1);
    simulnetx::splitmix64 b(2);
    EXPECT_NE(a.next_u64(), b.next_u64());
}

TEST(RngTest, NextDoubleStaysInUnitInterval) {
    simulnetx::splitmix64 rng(99);
    for (int i = 0; i < 10000; ++i) {
        const double d = rng.next_double();
        EXPECT_GE(d, 0.0);
        EXPECT_LT(d, 1.0);
    }
}

TEST(RngTest, NextDoubleIsDeterministicForSameSeed) {
    simulnetx::splitmix64 a(7);
    simulnetx::splitmix64 b(7);
    std::vector<double> first;
    std::vector<double> second;
    for (int i = 0; i < 100; ++i) {
        first.push_back(a.next_double());
        second.push_back(b.next_double());
    }
    EXPECT_EQ(first, second);
}

}  // namespace
