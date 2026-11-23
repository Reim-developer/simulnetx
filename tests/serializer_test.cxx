#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <tuple>
#include <utility>
#include <variant>
#include <vector>

#include <gtest/gtest.h>

#include "simulnetx/errors.hxx"
#include "simulnetx/serializer.hxx"

namespace {

using simulnetx::decode_error;
using simulnetx::serializable;
using simulnetx::serializer;

struct plain {
    int a;
    double b;
    bool operator==(const plain&) const = default;
};

struct vec3 {
    float x, y, z;
    bool operator==(const vec3&) const = default;
};

struct player {
    std::string name;
    vec3 pos;
    std::vector<std::uint32_t> items;
    bool operator==(const player&) const = default;
};

struct with_defaults {
    int a = 5;
    std::string s = "hi";
    bool operator==(const with_defaults&) const = default;
};

struct no_default {
    no_default(int v) : v(v) {}
    int v;
    bool operator==(const no_default&) const = default;
};

struct with_nodefault_members {
    int a;
    no_default first;
    no_default second;
    bool operator==(const with_nodefault_members&) const = default;
};

struct empty {
    bool operator==(const empty&) const = default;
};

struct tree_node {
    std::string name;
    std::vector<tree_node> children;
    int value;
    bool operator==(const tree_node&) const = default;
};

enum class color : std::uint8_t { red, green, blue };

// Not an aggregate (user ctor) and not trivially copyable (std::string
// member): nothing can serialize it without a specialization.
struct bad {
    bad() = default;
    std::string s;
};

}  // namespace

// A user specialization must win over the memcpy path (money is
// trivially-copyable, so without this the generic partial would apply).
// It encodes as the decimal string, observably different from 8 raw bytes.
namespace simulnetx {

struct money {
    money() = default;
    explicit money(long cents) : cents_(cents) {}
    [[nodiscard]] long cents() const { return cents_; }
    bool operator==(const money&) const = default;

private:
    long cents_ = 0;
};

template <>
struct serializer<money> {
    static void encode(const money& value, std::vector<std::byte>& out) {
        serializer<std::string>::encode(std::to_string(value.cents()), out);
    }

    static money decode(detail::cursor& in) {
        return money(std::stol(serializer<std::string>::decode(in)));
    }
};

}  // namespace simulnetx

namespace {

using simulnetx::money;

template <typename T>
std::vector<std::byte> encode(const T& value) {
    std::vector<std::byte> bytes;
    serializer<T>::encode(value, bytes);
    return bytes;
}

// Decodes from a whole buffer and asserts exact consumption: a well-formed
// payload has no trailing bytes.
template <typename T>
T round_trip(const T& value) {
    const std::vector<std::byte> bytes = encode(value);
    simulnetx::detail::cursor in(bytes.data(), bytes.size());
    T decoded = serializer<T>::decode(in);
    EXPECT_EQ(in.remaining(), 0u);
    return decoded;
}

template <typename T>
T decode_from(const std::vector<std::byte>& bytes) {
    static const std::byte dummy{};
    simulnetx::detail::cursor in(bytes.empty() ? &dummy : bytes.data(), bytes.size());
    return serializer<T>::decode(in);
}

static_assert(serializable<int>);
static_assert(serializable<double>);
static_assert(serializable<bool>);
static_assert(serializable<color>);
static_assert(serializable<const int>);
static_assert(serializable<std::string>);
static_assert(serializable<std::vector<int>>);
static_assert(serializable<std::optional<int>>);
static_assert(serializable<std::variant<int, std::string>>);
static_assert(serializable<std::pair<int, std::string>>);
static_assert(serializable<std::tuple<int, std::string, double>>);
static_assert(serializable<std::array<int, 3>>);
static_assert(serializable<plain>);
static_assert(serializable<player>);
static_assert(serializable<tree_node>);
static_assert(serializable<empty>);
static_assert(serializable<money>);

static_assert(!serializable<int*>);
static_assert(!serializable<const char*>);
static_assert(!serializable<void (*)()>);
static_assert(!serializable<int tree_node::*>);
static_assert(!serializable<bad>);
// Arrays cannot be returned by value, so they have no serializer - and
// the exclusion must be total, because node::send<T> is considered for
// string literals (T = char[N]) during overload resolution.
static_assert(!serializable<int[3]>);
static_assert(!serializable<char[3]>);

TEST(SerializerTest, TriviallyCopyableRoundTrip) {
    EXPECT_EQ(round_trip(42), 42);
    EXPECT_EQ(round_trip(-1.5), -1.5);
    EXPECT_EQ(round_trip(true), true);
    EXPECT_EQ(round_trip(std::uint64_t{0xDEADBEEFCAFEBABEULL}),
              0xDEADBEEFCAFEBABEULL);
    EXPECT_EQ(round_trip(color::green), color::green);
    EXPECT_EQ((round_trip(plain{3, 4.5})), (plain{3, 4.5}));
    EXPECT_EQ(round_trip(empty{}), empty{});
}

TEST(SerializerTest, StringRoundTrip) {
    EXPECT_EQ(round_trip(std::string{}), "");
    EXPECT_EQ(round_trip(std::string{"hello, simulnetx"}), "hello, simulnetx");
    // Length-prefixed, so an embedded NUL survives.
    EXPECT_EQ(round_trip(std::string{"a\0b", 3}), (std::string{"a\0b", 3}));
}

TEST(SerializerTest, VectorRoundTrip) {
    EXPECT_EQ((round_trip(std::vector<int>{})), (std::vector<int>{}));
    EXPECT_EQ((round_trip(std::vector<int>{1, 2, 3})), (std::vector<int>{1, 2, 3}));
    EXPECT_EQ((round_trip(std::vector<std::string>{"a", "bb", "ccc"})),
              (std::vector<std::string>{"a", "bb", "ccc"}));
    // Nested vectors.
    EXPECT_EQ((round_trip(std::vector<std::vector<int>>{{1}, {2, 3}})),
              (std::vector<std::vector<int>>{{1}, {2, 3}}));
}

TEST(SerializerTest, OptionalRoundTrip) {
    EXPECT_EQ((round_trip(std::optional<int>{})), std::nullopt);
    EXPECT_EQ((round_trip(std::optional<int>{7})), std::optional<int>{7});
    EXPECT_EQ((round_trip(std::optional<std::string>{"x"})),
              (std::optional<std::string>{"x"}));
}

TEST(SerializerTest, VariantRoundTrip) {
    using int_or_string = std::variant<int, std::string>;
    EXPECT_EQ((round_trip(int_or_string{42})), (int_or_string{42}));
    EXPECT_EQ((round_trip(int_or_string{std::string{"hi"}})),
              (int_or_string{std::string{"hi"}}));
    // No alternative needs to be default-constructible.
    using nodefault_or_string = std::variant<no_default, std::string>;
    EXPECT_EQ((round_trip(nodefault_or_string{no_default{7}})),
              (nodefault_or_string{no_default{7}}));
    EXPECT_EQ((round_trip(nodefault_or_string{std::string{"s"}})),
              (nodefault_or_string{std::string{"s"}}));
}

TEST(SerializerTest, PairAndTupleRoundTrip) {
    EXPECT_EQ((round_trip(std::pair<int, std::string>{3, "x"})),
              (std::pair<int, std::string>{3, "x"}));
    EXPECT_EQ((round_trip(std::tuple<int, std::string, double>{1, "y", 2.5})),
              (std::tuple<int, std::string, double>{1, "y", 2.5}));
    EXPECT_EQ((round_trip(std::tuple<>{})), (std::tuple<>{}));
}

TEST(SerializerTest, ArrayRoundTrip) {
    EXPECT_EQ((round_trip(std::array<int, 3>{1, 2, 3})),
              (std::array<int, 3>{1, 2, 3}));
    EXPECT_EQ((round_trip(std::array<std::string, 2>{"a", "b"})),
              (std::array<std::string, 2>{"a", "b"}));
    // The element type needs no default constructor.
    EXPECT_EQ((round_trip(std::array<no_default, 2>{no_default{1}, no_default{2}})),
              (std::array<no_default, 2>{no_default{1}, no_default{2}}));
}

TEST(SerializerTest, AggregateRoundTrip) {
    const player p{"reim", {1.0f, 2.0f, 3.0f}, {7, 8, 9}};
    EXPECT_EQ(round_trip(p), p);
    EXPECT_EQ((round_trip(with_defaults{})), (with_defaults{}));
    EXPECT_EQ((round_trip(with_defaults{9, "yo"})), (with_defaults{9, "yo"}));
    EXPECT_EQ((round_trip(with_nodefault_members{1, no_default{2}, no_default{3}})),
              (with_nodefault_members{1, no_default{2}, no_default{3}}));
}

TEST(SerializerTest, RecursiveTypeRoundTrip) {
    const tree_node root{"root", {{"left", {}, 1}, {"right", {{"leaf", {}, 3}}, 2}}, 0};
    EXPECT_EQ(round_trip(root), root);
}

TEST(SerializerTest, CustomSpecializationWinsOverMemcpy) {
    const money m{1234};
    EXPECT_EQ(round_trip(m), m);
    // The decimal string "1234": 8-byte length + 4 chars = 12 bytes.
    // The memcpy path would have written 8 raw bytes.
    EXPECT_EQ(encode(m).size(), 12u);
}

TEST(SerializerTest, SameValueEncodesToSameBytes) {
    const player p{"reim", {1.0f, 2.0f, 3.0f}, {7, 8, 9}};
    EXPECT_EQ(encode(p), encode(p));
    EXPECT_EQ(encode(42), encode(42));
    EXPECT_EQ(encode(std::string{"abc"}), encode(std::string{"abc"}));
}

TEST(SerializerTest, TypeTagStableWithinBuildAndDistinct) {
    EXPECT_EQ(simulnetx::detail::type_tag<player>(), simulnetx::detail::type_tag<player>());
    EXPECT_NE(simulnetx::detail::type_tag<int>(), simulnetx::detail::type_tag<double>());
    EXPECT_NE(simulnetx::detail::type_tag<player>(), simulnetx::detail::type_tag<plain>());
    EXPECT_EQ(simulnetx::detail::type_tag<const player>(),
              simulnetx::detail::type_tag<player>());
}

TEST(SerializerTest, TruncationThrowsDecodeError) {
    // A string cut one byte short.
    std::vector<std::byte> bytes = encode(std::string{"hello"});
    bytes.pop_back();
    EXPECT_THROW(decode_from<std::string>(bytes), decode_error);

    // A vector whose length is present but whose elements are missing.
    bytes = encode(std::vector<int>{1, 2, 3});
    bytes.resize(8);
    EXPECT_THROW((decode_from<std::vector<int>>(bytes)), decode_error);

    // An empty buffer cannot even supply a u64.
    EXPECT_THROW(decode_from<int>(std::vector<std::byte>{}), decode_error);

    // A player cut in the middle of its vector.
    bytes = encode(player{"reim", {1.0f, 2.0f, 3.0f}, {7, 8, 9}});
    bytes.resize(bytes.size() - 2);
    EXPECT_THROW(decode_from<player>(bytes), decode_error);
}

TEST(SerializerTest, MalformedOptionalFlagThrows) {
    const std::vector<std::byte> bytes{std::byte{2}};
    EXPECT_THROW((decode_from<std::optional<int>>(bytes)), decode_error);
}

TEST(SerializerTest, VariantIndexOutOfRangeThrows) {
    std::vector<std::byte> bytes;
    simulnetx::detail::append_u64(bytes, 5);
    EXPECT_THROW((decode_from<std::variant<int, std::string>>(bytes)), decode_error);
}

TEST(SerializerTest, CursorReportsTrailingBytes) {
    std::vector<std::byte> bytes = encode(42);
    bytes.push_back(std::byte{0});
    bytes.push_back(std::byte{0});
    bytes.push_back(std::byte{0});
    bytes.push_back(std::byte{0});
    simulnetx::detail::cursor in(bytes.data(), bytes.size());
    EXPECT_EQ(serializer<int>::decode(in), 42);
    EXPECT_EQ(in.remaining(), 4u);
}

}  // namespace
