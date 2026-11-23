#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <tuple>
#include <type_traits>
#include <vector>

#include <gtest/gtest.h>

#include "simulnetx/reflect.hxx"

namespace {

using simulnetx::reflect::field_count;
using simulnetx::reflect::for_each_field;
using simulnetx::reflect::get;
using simulnetx::reflect::reflectable;
using simulnetx::reflect::to_tuple;

struct plain {
    int a;
    double b;
};

struct empty {};

struct one_field {
    int a;
};

struct with_string {
    std::string name;
    std::uint32_t id;
};

struct with_defaults {
    int a = 5;
    std::string s = "hi";
};

struct no_default {
    no_default(int v) : v(v) {}
    int v;
};

struct with_nodefault_members {
    int a;
    no_default first;
    no_default second;
};

struct vec3 {
    float x, y, z;
};

struct player {
    std::string name;
    vec3 pos;
    std::vector<std::uint32_t> items;
};

struct with_std_array {
    std::array<int, 3> values;
    int x;
};

struct with_const_member {
    const int x;
    int y;
};

struct with_moveonly_member {
    std::unique_ptr<int> p;
    int x;
};

struct with_ref_member {
    int& r;
    int x;
};

struct with_const_ref_member {
    const int& r;
    int x;
};

struct has_ctor {
    has_ctor(int a) : a(a) {}
    int a;
    int b;
};

union with_union {
    int a;
    float b;
};

struct sixty_four_fields {
    int f0, f1, f2, f3, f4, f5, f6, f7, f8, f9, f10, f11, f12, f13, f14, f15;
    int f16, f17, f18, f19, f20, f21, f22, f23, f24, f25, f26, f27, f28, f29, f30, f31;
    int f32, f33, f34, f35, f36, f37, f38, f39, f40, f41, f42, f43, f44, f45, f46, f47;
    int f48, f49, f50, f51, f52, f53, f54, f55, f56, f57, f58, f59, f60, f61, f62, f63;
};

struct sixty_five_fields {
    int f0, f1, f2, f3, f4, f5, f6, f7, f8, f9, f10, f11, f12, f13, f14, f15;
    int f16, f17, f18, f19, f20, f21, f22, f23, f24, f25, f26, f27, f28, f29, f30, f31;
    int f32, f33, f34, f35, f36, f37, f38, f39, f40, f41, f42, f43, f44, f45, f46, f47;
    int f48, f49, f50, f51, f52, f53, f54, f55, f56, f57, f58, f59, f60, f61, f62, f63;
    int f64;
};

struct two_hundred_fields {
    int f0, f1, f2, f3, f4, f5, f6, f7, f8, f9, f10, f11, f12, f13, f14, f15;
    int f16, f17, f18, f19, f20, f21, f22, f23, f24, f25, f26, f27, f28, f29, f30, f31;
    int f32, f33, f34, f35, f36, f37, f38, f39, f40, f41, f42, f43, f44, f45, f46, f47;
    int f48, f49, f50, f51, f52, f53, f54, f55, f56, f57, f58, f59, f60, f61, f62, f63;
    int f64, f65, f66, f67, f68, f69, f70, f71, f72, f73, f74, f75, f76, f77, f78, f79;
    int f80, f81, f82, f83, f84, f85, f86, f87, f88, f89, f90, f91, f92, f93, f94, f95;
    int f96, f97, f98, f99, f100, f101, f102, f103, f104, f105, f106, f107, f108, f109, f110, f111;
    int f112, f113, f114, f115, f116, f117, f118, f119, f120, f121, f122, f123, f124, f125, f126, f127;
    int f128, f129, f130, f131, f132, f133, f134, f135, f136, f137, f138, f139, f140, f141, f142, f143;
    int f144, f145, f146, f147, f148, f149, f150, f151, f152, f153, f154, f155, f156, f157, f158, f159;
    int f160, f161, f162, f163, f164, f165, f166, f167, f168, f169, f170, f171, f172, f173, f174, f175;
    int f176, f177, f178, f179, f180, f181, f182, f183, f184, f185, f186, f187, f188, f189, f190, f191;
    int f192, f193, f194, f195, f196, f197, f198, f199;
};

struct two_hundred_one_fields {
    int f0, f1, f2, f3, f4, f5, f6, f7, f8, f9, f10, f11, f12, f13, f14, f15;
    int f16, f17, f18, f19, f20, f21, f22, f23, f24, f25, f26, f27, f28, f29, f30, f31;
    int f32, f33, f34, f35, f36, f37, f38, f39, f40, f41, f42, f43, f44, f45, f46, f47;
    int f48, f49, f50, f51, f52, f53, f54, f55, f56, f57, f58, f59, f60, f61, f62, f63;
    int f64, f65, f66, f67, f68, f69, f70, f71, f72, f73, f74, f75, f76, f77, f78, f79;
    int f80, f81, f82, f83, f84, f85, f86, f87, f88, f89, f90, f91, f92, f93, f94, f95;
    int f96, f97, f98, f99, f100, f101, f102, f103, f104, f105, f106, f107, f108, f109, f110, f111;
    int f112, f113, f114, f115, f116, f117, f118, f119, f120, f121, f122, f123, f124, f125, f126, f127;
    int f128, f129, f130, f131, f132, f133, f134, f135, f136, f137, f138, f139, f140, f141, f142, f143;
    int f144, f145, f146, f147, f148, f149, f150, f151, f152, f153, f154, f155, f156, f157, f158, f159;
    int f160, f161, f162, f163, f164, f165, f166, f167, f168, f169, f170, f171, f172, f173, f174, f175;
    int f176, f177, f178, f179, f180, f181, f182, f183, f184, f185, f186, f187, f188, f189, f190, f191;
    int f192, f193, f194, f195, f196, f197, f198, f199, f200;
};

// Field counts, including the tricky cases: nested aggregates count as one
// field each (no brace-elision flattening), non-default-constructible
// members do not confuse the probe, and std::array is one member.
static_assert(field_count<plain>() == 2);
static_assert(field_count<empty>() == 0);
static_assert(field_count<one_field>() == 1);
static_assert(field_count<with_string>() == 2);
static_assert(field_count<with_defaults>() == 2);
static_assert(field_count<with_nodefault_members>() == 3);
static_assert(field_count<player>() == 3);
static_assert(field_count<with_std_array>() == 2);
static_assert(field_count<with_const_member>() == 2);
static_assert(field_count<with_moveonly_member>() == 2);
static_assert(field_count<sixty_four_fields>() == 64);
static_assert(field_count<sixty_five_fields>() == 65);
static_assert(field_count<two_hundred_fields>() == 200);
// Over the cap the probe reports 201 ("more than 200"), never a wrong count.
static_assert(field_count<two_hundred_one_fields>() == 201);

static_assert(reflectable<plain>);
static_assert(reflectable<empty>);
static_assert(reflectable<one_field>);
static_assert(reflectable<with_string>);
static_assert(reflectable<with_defaults>);
static_assert(reflectable<with_nodefault_members>);
static_assert(reflectable<player>);
static_assert(reflectable<with_std_array>);
static_assert(reflectable<with_const_member>);
static_assert(reflectable<with_moveonly_member>);
static_assert(reflectable<sixty_four_fields>);
static_assert(reflectable<sixty_five_fields>);
static_assert(reflectable<two_hundred_fields>);
static_assert(reflectable<std::array<int, 3>>);

static_assert(!reflectable<has_ctor>);
static_assert(!reflectable<with_union>);
static_assert(!reflectable<int[3]>);
static_assert(!reflectable<int>);
static_assert(!reflectable<std::string>);
static_assert(!reflectable<with_ref_member>);
static_assert(!reflectable<two_hundred_one_fields>);
// Const reference members satisfy the concept (a prvalue binds to a const
// reference, so the probe cannot see them); the serializer rejects them
// separately with a clear static_assert. See serializer_test.cxx.
static_assert(reflectable<with_const_ref_member>);

// The decltype backstop used by the serializer sees const references (the
// case the concept cannot catch: a prvalue binds to a const reference, so
// the count is the true field count and the binding has reference type).
// It is NOT asserted for non-const references: their paren count is
// polluted (1 via the copy constructor), so instantiating the backstop
// there is a hard structured-binding error, not a value. The concept
// excludes those structs before the serializer ever consults it.
static_assert(simulnetx::reflect::detail::has_reference_field_decltype_v<with_const_ref_member>);
static_assert(!simulnetx::reflect::detail::has_reference_field_decltype_v<plain>);
// The backstop instantiates the arity-200 generated overload.
static_assert(!simulnetx::reflect::detail::has_reference_field_decltype_v<two_hundred_fields>);

TEST(ReflectTest, GetReturnsReferencesInOrder) {
    player p{"reim", {1.0f, 2.0f, 3.0f}, {7, 8}};
    EXPECT_EQ(get<0>(p), "reim");
    EXPECT_EQ(get<1>(p).x, 1.0f);
    EXPECT_EQ(get<2>(p).size(), 2u);
    // get returns a reference: mutation through it lands in the object.
    get<0>(p) = "changed";
    EXPECT_EQ(p.name, "changed");
    get<1>(p).y = 9.0f;
    EXPECT_EQ(p.pos.y, 9.0f);
}

TEST(ReflectTest, GetOnConstObjectIsConst) {
    const plain p{3, 4.5};
    static_assert(std::is_same_v<decltype(get<0>(p)), const int&>);
    static_assert(std::is_same_v<decltype(get<1>(p)), const double&>);
    EXPECT_EQ(get<0>(p), 3);
    EXPECT_EQ(get<1>(p), 4.5);
}

TEST(ReflectTest, ToTupleRoundTrips) {
    with_string w{"hello", 42};
    auto t = to_tuple(w);
    static_assert(std::tuple_size_v<decltype(t)> == 2);
    EXPECT_EQ(std::get<0>(t), "hello");
    EXPECT_EQ(std::get<1>(t), 42u);
    // The tuple holds references, not copies.
    std::get<0>(t) = "world";
    EXPECT_EQ(w.name, "world");
}

TEST(ReflectTest, ToTupleOfEmptyIsEmpty) {
    empty e;
    auto t = to_tuple(e);
    static_assert(std::tuple_size_v<decltype(t)> == 0);
}

TEST(ReflectTest, ForEachFieldVisitsInOrderAndMutates) {
    plain p{1, 2.0};
    int seen = 0;
    double sum = 0.0;
    for_each_field(p, [&](auto& field) {
        ++seen;
        sum += static_cast<double>(field);
    });
    EXPECT_EQ(seen, 2);
    EXPECT_EQ(sum, 3.0);

    // Mutation through for_each_field.
    for_each_field(p, [](auto& field) { field += field; });
    EXPECT_EQ(p.a, 2);
    EXPECT_EQ(p.b, 4.0);
}

TEST(ReflectTest, ForEachFieldOnSixtyFourFields) {
    sixty_four_fields s{};
    int count = 0;
    for_each_field(s, [&](int& field) {
        field = count;
        ++count;
    });
    EXPECT_EQ(count, 64);
    EXPECT_EQ(s.f0, 0);
    EXPECT_EQ(s.f63, 63);
    EXPECT_EQ(get<63>(s), 63);
}

TEST(ReflectTest, ForEachFieldOnTwoHundredFields) {
    two_hundred_fields s{};
    int count = 0;
    for_each_field(s, [&](int& field) {
        field = count;
        ++count;
    });
    EXPECT_EQ(count, 200);
    EXPECT_EQ(s.f0, 0);
    EXPECT_EQ(s.f199, 199);
    EXPECT_EQ(get<199>(s), 199);
}

TEST(ReflectTest, GetOnRvalueWorks) {
    EXPECT_EQ(get<0>(plain{5, 6.5}), 5);
    EXPECT_EQ(get<1>(plain{5, 6.5}), 6.5);
}

}  // namespace
