#include <cstddef>
#include <string>

#include <gtest/gtest.h>

#include "simulnetx/expectation.hxx"
#include "simulnetx/network.hxx"

namespace {

TEST(ExpectationTest, PassingChecksDoNotThrow) {
    EXPECT_NO_THROW(simulnetx::expect(5).equal(5));
    EXPECT_NO_THROW(simulnetx::expect(5).not_equal(6));
    EXPECT_NO_THROW(simulnetx::expect(5).greater_than(4));
    EXPECT_NO_THROW(simulnetx::expect(5).less_than(6));
    EXPECT_NO_THROW(simulnetx::expect(5).greater_or_equal(5));
    EXPECT_NO_THROW(simulnetx::expect(5).less_or_equal(5));
    EXPECT_NO_THROW(simulnetx::expect(5).between(1, 10));
    EXPECT_NO_THROW(simulnetx::expect(5).between(5, 5));
}

TEST(ExpectationTest, FailingChecksThrowAssertionFailed) {
    EXPECT_THROW(simulnetx::expect(5).equal(6), simulnetx::assertion_failed);
    EXPECT_THROW(simulnetx::expect(5).not_equal(5), simulnetx::assertion_failed);
    EXPECT_THROW(simulnetx::expect(5).greater_than(5), simulnetx::assertion_failed);
    EXPECT_THROW(simulnetx::expect(5).less_than(5), simulnetx::assertion_failed);
    EXPECT_THROW(simulnetx::expect(5).greater_or_equal(6), simulnetx::assertion_failed);
    EXPECT_THROW(simulnetx::expect(5).less_or_equal(4), simulnetx::assertion_failed);
    EXPECT_THROW(simulnetx::expect(5).between(6, 10), simulnetx::assertion_failed);
    EXPECT_THROW(simulnetx::expect(5).between(1, 4), simulnetx::assertion_failed);
}

TEST(ExpectationTest, ChecksChain) {
    EXPECT_NO_THROW(simulnetx::expect(5).greater_than(1).less_than(10).between(0, 100));
}

TEST(ExpectationTest, ImplicitConversionToPlainValue) {
    const std::size_t n = simulnetx::expect<std::size_t>(7);
    EXPECT_EQ(n, 7u);
    const int m = simulnetx::expect(3) + 4;
    EXPECT_EQ(m, 7);
}

TEST(ExpectationTest, FreeExpectMessageHasExpectedAndActual) {
    try {
        simulnetx::expect(1).equal(2);
        FAIL() << "expected assertion_failed";
    } catch (const simulnetx::assertion_failed& e) {
        EXPECT_EQ(std::string(e.what()), "expected value == 2, got 1");
    }
}

TEST(ExpectationTest, NetworkExpectationMessageCarriesSeedAndVirtualTime) {
    simulnetx::network net(42);
    net.run_until(std::chrono::nanoseconds{10500000000LL});
    try {
        net.delivered_count().equal(1);
        FAIL() << "expected assertion_failed";
    } catch (const simulnetx::assertion_failed& e) {
        EXPECT_EQ(std::string(e.what()),
                  "expected delivered_count == 1, got 0 (seed 42, t=10500000000ns)");
    }
}

TEST(ExpectationTest, NetworkExpectationPassesOnCorrectValue) {
    simulnetx::network net(42);
    auto& a = net.add_node("A");
    auto& b = net.add_node("B");
    net.link(a.id(), b.id(), {});
    a.send(b.id(), "x");
    net.run_for(std::chrono::seconds{1});
    EXPECT_NO_THROW(net.sent_count().equal(1));
    EXPECT_NO_THROW(net.delivered_count().equal(1));
    EXPECT_NO_THROW(net.dropped_count().equal(0));
}

}  // namespace
