#include <stdexcept>

#include <gtest/gtest.h>

#include "simulnetx/network.hxx"
#include "simulnetx/udp.hxx"

namespace {

using simulnetx::udp::endpoint;
using simulnetx::udp::socket;

TEST(SocketTest, EndpointEqualityAndToString) {
    EXPECT_EQ((endpoint{3, 8080}), (endpoint{3, 8080}));
    EXPECT_NE((endpoint{3, 8080}), (endpoint{3, 8081}));
    EXPECT_NE((endpoint{3, 8080}), (endpoint{4, 8080}));
    EXPECT_EQ(simulnetx::udp::to_string(endpoint{3, 8080}), "node#3:8080");
    EXPECT_EQ(simulnetx::udp::to_string(endpoint{0, 0}), "node#0:0");
}

TEST(SocketTest, BindAndLocalEndpoint) {
    simulnetx::network net(42);
    auto& a = net.add_node("a");
    socket s(net, a);
    EXPECT_TRUE(s.is_open());
    EXPECT_THROW(s.local_endpoint(), std::logic_error);
    s.bind(8080);
    EXPECT_EQ(s.local_endpoint(), (endpoint{a.id(), 8080}));
}

TEST(SocketTest, BindConflictOnSameNodeThrows) {
    simulnetx::network net(42);
    auto& a = net.add_node("a");
    socket s1(net, a);
    s1.bind(8080);
    socket s2(net, a);
    EXPECT_THROW(s2.bind(8080), std::logic_error);
}

TEST(SocketTest, SamePortOnDifferentNodesIsFine) {
    simulnetx::network net(42);
    auto& a = net.add_node("a");
    auto& b = net.add_node("b");
    socket s1(net, a);
    socket s2(net, b);
    s1.bind(8080);
    EXPECT_NO_THROW(s2.bind(8080));
    EXPECT_EQ(s2.local_endpoint(), (endpoint{b.id(), 8080}));
}

TEST(SocketTest, DoubleBindThrows) {
    simulnetx::network net(42);
    auto& a = net.add_node("a");
    socket s(net, a);
    s.bind(8080);
    EXPECT_THROW(s.bind(9090), std::logic_error);
    EXPECT_THROW(s.bind(8080), std::logic_error);
    EXPECT_EQ(s.local_endpoint().port, 8080);
}

TEST(SocketTest, EphemeralPortsArePerNodeFrom49152) {
    simulnetx::network net(42);
    auto& a = net.add_node("a");
    auto& b = net.add_node("b");
    socket e1(net, a);
    socket e2(net, a);
    socket e3(net, b);
    e1.bind(0);
    e2.bind(0);
    e3.bind(0);
    EXPECT_EQ(e1.local_endpoint().port, 49152);
    EXPECT_EQ(e2.local_endpoint().port, 49153);
    EXPECT_EQ(e3.local_endpoint().port, 49152);
}

TEST(SocketTest, CloseUnregistersAndIsIdempotent) {
    simulnetx::network net(42);
    auto& a = net.add_node("a");
    socket s(net, a);
    s.bind(8080);
    s.close();
    EXPECT_FALSE(s.is_open());
    EXPECT_NO_THROW(s.close());
    EXPECT_THROW(s.bind(9090), std::logic_error);
    // The port is free again: a new socket can bind it.
    socket s2(net, a);
    EXPECT_NO_THROW(s2.bind(8080));
}

TEST(SocketTest, DestructorUnregisters) {
    simulnetx::network net(42);
    auto& a = net.add_node("a");
    {
        socket s(net, a);
        s.bind(8080);
    }
    socket s2(net, a);
    EXPECT_NO_THROW(s2.bind(8080));
}

TEST(SocketTest, MoveTransfersBinding) {
    simulnetx::network net(42);
    auto& a = net.add_node("a");
    socket orig(net, a);
    orig.bind(8080);
    socket moved(std::move(orig));
    EXPECT_FALSE(orig.is_open());
    EXPECT_TRUE(moved.is_open());
    EXPECT_EQ(moved.local_endpoint(), (endpoint{a.id(), 8080}));
    // The registry entry moved with the socket: the port is still taken.
    socket other(net, a);
    EXPECT_THROW(other.bind(8080), std::logic_error);
    // And closing the moved socket frees it.
    moved.close();
    EXPECT_NO_THROW(other.bind(8080));
}

TEST(SocketTest, MoveAssignmentTransfersBinding) {
    simulnetx::network net(42);
    auto& a = net.add_node("a");
    socket orig(net, a);
    orig.bind(8080);
    socket target(net, a);
    target.bind(9090);
    target = std::move(orig);
    EXPECT_FALSE(orig.is_open());
    EXPECT_EQ(target.local_endpoint(), (endpoint{a.id(), 8080}));
    // The target's old binding was released by the assignment.
    socket other(net, a);
    EXPECT_NO_THROW(other.bind(9090));
    EXPECT_THROW(other.bind(8080), std::logic_error);
}

TEST(SocketTest, MoveOfUnboundSocket) {
    simulnetx::network net(42);
    auto& a = net.add_node("a");
    socket orig(net, a);
    socket moved(std::move(orig));
    EXPECT_FALSE(orig.is_open());
    EXPECT_TRUE(moved.is_open());
    EXPECT_THROW(moved.local_endpoint(), std::logic_error);
    EXPECT_NO_THROW(moved.bind(8080));
}

}  // namespace
