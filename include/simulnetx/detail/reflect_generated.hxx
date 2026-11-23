// Generated at author time by a throwaway script. DO NOT EDIT BY HAND.
// (Regenerate from scratch if the arity cap changes; the generator is
// intentionally not part of the repo - no Python in the repo.)
//
// Per-arity structured-binding machinery for simulnetx::reflect:
// to_tuple_impl decomposes an aggregate into a tuple of references,
// and has_reference_field_tag detects reference members via decltype
// of the bindings (a reference member's binding has reference type,
// a value member's does not). Both are needed because structured
// bindings cannot be indexed: each arity needs its own spelling.

#pragma once

#include <cstddef>
#include <tuple>
#include <type_traits>
#include <utility>

namespace simulnetx::reflect::detail {

// Arity 0: decompose into a tuple of references.
template <typename T>
constexpr auto to_tuple_impl(T&&, std::integral_constant<std::size_t, 0>) {
    return std::tuple<>{};
}

// Arity 0: reference-member detection for the serializer backstop.
template <typename T>
constexpr std::false_type has_reference_field_tag(T&, std::integral_constant<std::size_t, 0>);

// Arity 1: decompose into a tuple of references.
template <typename T>
constexpr auto to_tuple_impl(T&& obj, std::integral_constant<std::size_t, 1>) {
    auto&& [f0] = std::forward<T>(obj);
    return std::forward_as_tuple(f0);
}

// Arity 1: reference-member detection for the serializer backstop.
template <typename T>
constexpr auto has_reference_field_tag(T&, std::integral_constant<std::size_t, 1>)
    -> decltype([](T& u) {
           auto& [f0] = u;
           return std::bool_constant<(std::is_reference_v<decltype(f0)>)>{};
       }(std::declval<T&>()));

// Arity 2: decompose into a tuple of references.
template <typename T>
constexpr auto to_tuple_impl(T&& obj, std::integral_constant<std::size_t, 2>) {
    auto&& [f0, f1] = std::forward<T>(obj);
    return std::forward_as_tuple(f0, f1);
}

// Arity 2: reference-member detection for the serializer backstop.
template <typename T>
constexpr auto has_reference_field_tag(T&, std::integral_constant<std::size_t, 2>)
    -> decltype([](T& u) {
           auto& [f0, f1] = u;
           return std::bool_constant<(std::is_reference_v<decltype(f0)> || std::is_reference_v<decltype(f1)>)>{};
       }(std::declval<T&>()));

// Arity 3: decompose into a tuple of references.
template <typename T>
constexpr auto to_tuple_impl(T&& obj, std::integral_constant<std::size_t, 3>) {
    auto&& [f0, f1, f2] = std::forward<T>(obj);
    return std::forward_as_tuple(f0, f1, f2);
}

// Arity 3: reference-member detection for the serializer backstop.
template <typename T>
constexpr auto has_reference_field_tag(T&, std::integral_constant<std::size_t, 3>)
    -> decltype([](T& u) {
           auto& [f0, f1, f2] = u;
           return std::bool_constant<(std::is_reference_v<decltype(f0)> || std::is_reference_v<decltype(f1)> || std::is_reference_v<decltype(f2)>)>{};
       }(std::declval<T&>()));

// Arity 4: decompose into a tuple of references.
template <typename T>
constexpr auto to_tuple_impl(T&& obj, std::integral_constant<std::size_t, 4>) {
    auto&& [f0, f1, f2, f3] = std::forward<T>(obj);
    return std::forward_as_tuple(f0, f1, f2, f3);
}

// Arity 4: reference-member detection for the serializer backstop.
template <typename T>
constexpr auto has_reference_field_tag(T&, std::integral_constant<std::size_t, 4>)
    -> decltype([](T& u) {
           auto& [f0, f1, f2, f3] = u;
           return std::bool_constant<(std::is_reference_v<decltype(f0)> || std::is_reference_v<decltype(f1)> || std::is_reference_v<decltype(f2)> || std::is_reference_v<decltype(f3)>)>{};
       }(std::declval<T&>()));

// Arity 5: decompose into a tuple of references.
template <typename T>
constexpr auto to_tuple_impl(T&& obj, std::integral_constant<std::size_t, 5>) {
    auto&& [f0, f1, f2, f3, f4] = std::forward<T>(obj);
    return std::forward_as_tuple(f0, f1, f2, f3, f4);
}

// Arity 5: reference-member detection for the serializer backstop.
template <typename T>
constexpr auto has_reference_field_tag(T&, std::integral_constant<std::size_t, 5>)
    -> decltype([](T& u) {
           auto& [f0, f1, f2, f3, f4] = u;
           return std::bool_constant<(std::is_reference_v<decltype(f0)> || std::is_reference_v<decltype(f1)> || std::is_reference_v<decltype(f2)> || std::is_reference_v<decltype(f3)> || std::is_reference_v<decltype(f4)>)>{};
       }(std::declval<T&>()));

// Arity 6: decompose into a tuple of references.
template <typename T>
constexpr auto to_tuple_impl(T&& obj, std::integral_constant<std::size_t, 6>) {
    auto&& [f0, f1, f2, f3, f4, f5] = std::forward<T>(obj);
    return std::forward_as_tuple(f0, f1, f2, f3, f4, f5);
}

// Arity 6: reference-member detection for the serializer backstop.
template <typename T>
constexpr auto has_reference_field_tag(T&, std::integral_constant<std::size_t, 6>)
    -> decltype([](T& u) {
           auto& [f0, f1, f2, f3, f4, f5] = u;
           return std::bool_constant<(std::is_reference_v<decltype(f0)> || std::is_reference_v<decltype(f1)> || std::is_reference_v<decltype(f2)> || std::is_reference_v<decltype(f3)> || std::is_reference_v<decltype(f4)> || std::is_reference_v<decltype(f5)>)>{};
       }(std::declval<T&>()));

// Arity 7: decompose into a tuple of references.
template <typename T>
constexpr auto to_tuple_impl(T&& obj, std::integral_constant<std::size_t, 7>) {
    auto&& [f0, f1, f2, f3, f4, f5, f6] = std::forward<T>(obj);
    return std::forward_as_tuple(f0, f1, f2, f3, f4, f5, f6);
}

// Arity 7: reference-member detection for the serializer backstop.
template <typename T>
constexpr auto has_reference_field_tag(T&, std::integral_constant<std::size_t, 7>)
    -> decltype([](T& u) {
           auto& [f0, f1, f2, f3, f4, f5, f6] = u;
           return std::bool_constant<(std::is_reference_v<decltype(f0)> || std::is_reference_v<decltype(f1)> || std::is_reference_v<decltype(f2)> || std::is_reference_v<decltype(f3)> || std::is_reference_v<decltype(f4)> || std::is_reference_v<decltype(f5)> || std::is_reference_v<decltype(f6)>)>{};
       }(std::declval<T&>()));

// Arity 8: decompose into a tuple of references.
template <typename T>
constexpr auto to_tuple_impl(T&& obj, std::integral_constant<std::size_t, 8>) {
    auto&& [f0, f1, f2, f3, f4, f5, f6, f7] = std::forward<T>(obj);
    return std::forward_as_tuple(f0, f1, f2, f3, f4, f5, f6, f7);
}

// Arity 8: reference-member detection for the serializer backstop.
template <typename T>
constexpr auto has_reference_field_tag(T&, std::integral_constant<std::size_t, 8>)
    -> decltype([](T& u) {
           auto& [f0, f1, f2, f3, f4, f5, f6, f7] = u;
           return std::bool_constant<(std::is_reference_v<decltype(f0)> || std::is_reference_v<decltype(f1)> || std::is_reference_v<decltype(f2)> || std::is_reference_v<decltype(f3)> || std::is_reference_v<decltype(f4)> || std::is_reference_v<decltype(f5)> || std::is_reference_v<decltype(f6)> || std::is_reference_v<decltype(f7)>)>{};
       }(std::declval<T&>()));

// Arity 9: decompose into a tuple of references.
template <typename T>
constexpr auto to_tuple_impl(T&& obj, std::integral_constant<std::size_t, 9>) {
    auto&& [f0, f1, f2, f3, f4, f5, f6, f7, f8] = std::forward<T>(obj);
    return std::forward_as_tuple(f0, f1, f2, f3, f4, f5, f6, f7, f8);
}

// Arity 9: reference-member detection for the serializer backstop.
template <typename T>
constexpr auto has_reference_field_tag(T&, std::integral_constant<std::size_t, 9>)
    -> decltype([](T& u) {
           auto& [f0, f1, f2, f3, f4, f5, f6, f7, f8] = u;
           return std::bool_constant<(std::is_reference_v<decltype(f0)> || std::is_reference_v<decltype(f1)> || std::is_reference_v<decltype(f2)> || std::is_reference_v<decltype(f3)> || std::is_reference_v<decltype(f4)> || std::is_reference_v<decltype(f5)> || std::is_reference_v<decltype(f6)> || std::is_reference_v<decltype(f7)> || std::is_reference_v<decltype(f8)>)>{};
       }(std::declval<T&>()));

// Arity 10: decompose into a tuple of references.
template <typename T>
constexpr auto to_tuple_impl(T&& obj, std::integral_constant<std::size_t, 10>) {
    auto&& [f0, f1, f2, f3, f4, f5, f6, f7, f8, f9] = std::forward<T>(obj);
    return std::forward_as_tuple(f0, f1, f2, f3, f4, f5, f6, f7, f8, f9);
}

// Arity 10: reference-member detection for the serializer backstop.
template <typename T>
constexpr auto has_reference_field_tag(T&, std::integral_constant<std::size_t, 10>)
    -> decltype([](T& u) {
           auto& [f0, f1, f2, f3, f4, f5, f6, f7, f8, f9] = u;
           return std::bool_constant<(std::is_reference_v<decltype(f0)> || std::is_reference_v<decltype(f1)> || std::is_reference_v<decltype(f2)> || std::is_reference_v<decltype(f3)> || std::is_reference_v<decltype(f4)> || std::is_reference_v<decltype(f5)> || std::is_reference_v<decltype(f6)> || std::is_reference_v<decltype(f7)> || std::is_reference_v<decltype(f8)> || std::is_reference_v<decltype(f9)>)>{};
       }(std::declval<T&>()));

// Arity 11: decompose into a tuple of references.
template <typename T>
constexpr auto to_tuple_impl(T&& obj, std::integral_constant<std::size_t, 11>) {
    auto&& [f0, f1, f2, f3, f4, f5, f6, f7, f8, f9, f10] = std::forward<T>(obj);
    return std::forward_as_tuple(f0, f1, f2, f3, f4, f5, f6, f7, f8, f9, f10);
}

// Arity 11: reference-member detection for the serializer backstop.
template <typename T>
constexpr auto has_reference_field_tag(T&, std::integral_constant<std::size_t, 11>)
    -> decltype([](T& u) {
           auto& [f0, f1, f2, f3, f4, f5, f6, f7, f8, f9, f10] = u;
           return std::bool_constant<(std::is_reference_v<decltype(f0)> || std::is_reference_v<decltype(f1)> || std::is_reference_v<decltype(f2)> || std::is_reference_v<decltype(f3)> || std::is_reference_v<decltype(f4)> || std::is_reference_v<decltype(f5)> || std::is_reference_v<decltype(f6)> || std::is_reference_v<decltype(f7)> || std::is_reference_v<decltype(f8)> || std::is_reference_v<decltype(f9)> || std::is_reference_v<decltype(f10)>)>{};
       }(std::declval<T&>()));

// Arity 12: decompose into a tuple of references.
template <typename T>
constexpr auto to_tuple_impl(T&& obj, std::integral_constant<std::size_t, 12>) {
    auto&& [f0, f1, f2, f3, f4, f5, f6, f7, f8, f9, f10, f11] = std::forward<T>(obj);
    return std::forward_as_tuple(f0, f1, f2, f3, f4, f5, f6, f7, f8, f9, f10, f11);
}

// Arity 12: reference-member detection for the serializer backstop.
template <typename T>
constexpr auto has_reference_field_tag(T&, std::integral_constant<std::size_t, 12>)
    -> decltype([](T& u) {
           auto& [f0, f1, f2, f3, f4, f5, f6, f7, f8, f9, f10, f11] = u;
           return std::bool_constant<(std::is_reference_v<decltype(f0)> || std::is_reference_v<decltype(f1)> || std::is_reference_v<decltype(f2)> || std::is_reference_v<decltype(f3)> || std::is_reference_v<decltype(f4)> || std::is_reference_v<decltype(f5)> || std::is_reference_v<decltype(f6)> || std::is_reference_v<decltype(f7)> || std::is_reference_v<decltype(f8)> || std::is_reference_v<decltype(f9)> || std::is_reference_v<decltype(f10)> || std::is_reference_v<decltype(f11)>)>{};
       }(std::declval<T&>()));

// Arity 13: decompose into a tuple of references.
template <typename T>
constexpr auto to_tuple_impl(T&& obj, std::integral_constant<std::size_t, 13>) {
    auto&& [f0, f1, f2, f3, f4, f5, f6, f7, f8, f9, f10, f11, f12] = std::forward<T>(obj);
    return std::forward_as_tuple(f0, f1, f2, f3, f4, f5, f6, f7, f8, f9, f10, f11, f12);
}

// Arity 13: reference-member detection for the serializer backstop.
template <typename T>
constexpr auto has_reference_field_tag(T&, std::integral_constant<std::size_t, 13>)
    -> decltype([](T& u) {
           auto& [f0, f1, f2, f3, f4, f5, f6, f7, f8, f9, f10, f11, f12] = u;
           return std::bool_constant<(std::is_reference_v<decltype(f0)> || std::is_reference_v<decltype(f1)> || std::is_reference_v<decltype(f2)> || std::is_reference_v<decltype(f3)> || std::is_reference_v<decltype(f4)> || std::is_reference_v<decltype(f5)> || std::is_reference_v<decltype(f6)> || std::is_reference_v<decltype(f7)> || std::is_reference_v<decltype(f8)> || std::is_reference_v<decltype(f9)> || std::is_reference_v<decltype(f10)> || std::is_reference_v<decltype(f11)> || std::is_reference_v<decltype(f12)>)>{};
       }(std::declval<T&>()));

// Arity 14: decompose into a tuple of references.
template <typename T>
constexpr auto to_tuple_impl(T&& obj, std::integral_constant<std::size_t, 14>) {
    auto&& [f0, f1, f2, f3, f4, f5, f6, f7, f8, f9, f10, f11, f12, f13] = std::forward<T>(obj);
    return std::forward_as_tuple(f0, f1, f2, f3, f4, f5, f6, f7, f8, f9, f10, f11, f12, f13);
}

// Arity 14: reference-member detection for the serializer backstop.
template <typename T>
constexpr auto has_reference_field_tag(T&, std::integral_constant<std::size_t, 14>)
    -> decltype([](T& u) {
           auto& [f0, f1, f2, f3, f4, f5, f6, f7, f8, f9, f10, f11, f12, f13] = u;
           return std::bool_constant<(std::is_reference_v<decltype(f0)> || std::is_reference_v<decltype(f1)> || std::is_reference_v<decltype(f2)> || std::is_reference_v<decltype(f3)> || std::is_reference_v<decltype(f4)> || std::is_reference_v<decltype(f5)> || std::is_reference_v<decltype(f6)> || std::is_reference_v<decltype(f7)> || std::is_reference_v<decltype(f8)> || std::is_reference_v<decltype(f9)> || std::is_reference_v<decltype(f10)> || std::is_reference_v<decltype(f11)> || std::is_reference_v<decltype(f12)> || std::is_reference_v<decltype(f13)>)>{};
       }(std::declval<T&>()));

// Arity 15: decompose into a tuple of references.
template <typename T>
constexpr auto to_tuple_impl(T&& obj, std::integral_constant<std::size_t, 15>) {
    auto&& [f0, f1, f2, f3, f4, f5, f6, f7, f8, f9, f10, f11, f12, f13, f14] = std::forward<T>(obj);
    return std::forward_as_tuple(f0, f1, f2, f3, f4, f5, f6, f7, f8, f9, f10, f11, f12, f13, f14);
}

// Arity 15: reference-member detection for the serializer backstop.
template <typename T>
constexpr auto has_reference_field_tag(T&, std::integral_constant<std::size_t, 15>)
    -> decltype([](T& u) {
           auto& [f0, f1, f2, f3, f4, f5, f6, f7, f8, f9, f10, f11, f12, f13, f14] = u;
           return std::bool_constant<(std::is_reference_v<decltype(f0)> || std::is_reference_v<decltype(f1)> || std::is_reference_v<decltype(f2)> || std::is_reference_v<decltype(f3)> || std::is_reference_v<decltype(f4)> || std::is_reference_v<decltype(f5)> || std::is_reference_v<decltype(f6)> || std::is_reference_v<decltype(f7)> || std::is_reference_v<decltype(f8)> || std::is_reference_v<decltype(f9)> || std::is_reference_v<decltype(f10)> || std::is_reference_v<decltype(f11)> || std::is_reference_v<decltype(f12)> || std::is_reference_v<decltype(f13)> || std::is_reference_v<decltype(f14)>)>{};
       }(std::declval<T&>()));

// Arity 16: decompose into a tuple of references.
template <typename T>
constexpr auto to_tuple_impl(T&& obj, std::integral_constant<std::size_t, 16>) {
    auto&& [f0, f1, f2, f3, f4, f5, f6, f7, f8, f9, f10, f11, f12, f13, f14, f15] = std::forward<T>(obj);
    return std::forward_as_tuple(f0, f1, f2, f3, f4, f5, f6, f7, f8, f9, f10, f11, f12, f13, f14, f15);
}

// Arity 16: reference-member detection for the serializer backstop.
template <typename T>
constexpr auto has_reference_field_tag(T&, std::integral_constant<std::size_t, 16>)
    -> decltype([](T& u) {
           auto& [f0, f1, f2, f3, f4, f5, f6, f7, f8, f9, f10, f11, f12, f13, f14, f15] = u;
           return std::bool_constant<(std::is_reference_v<decltype(f0)> || std::is_reference_v<decltype(f1)> || std::is_reference_v<decltype(f2)> || std::is_reference_v<decltype(f3)> || std::is_reference_v<decltype(f4)> || std::is_reference_v<decltype(f5)> || std::is_reference_v<decltype(f6)> || std::is_reference_v<decltype(f7)> || std::is_reference_v<decltype(f8)> || std::is_reference_v<decltype(f9)> || std::is_reference_v<decltype(f10)> || std::is_reference_v<decltype(f11)> || std::is_reference_v<decltype(f12)> || std::is_reference_v<decltype(f13)> || std::is_reference_v<decltype(f14)> || std::is_reference_v<decltype(f15)>)>{};
       }(std::declval<T&>()));

// Arity 17: decompose into a tuple of references.
template <typename T>
constexpr auto to_tuple_impl(T&& obj, std::integral_constant<std::size_t, 17>) {
    auto&& [f0, f1, f2, f3, f4, f5, f6, f7, f8, f9, f10, f11, f12, f13, f14, f15, f16] = std::forward<T>(obj);
    return std::forward_as_tuple(f0, f1, f2, f3, f4, f5, f6, f7, f8, f9, f10, f11, f12, f13, f14, f15, f16);
}

// Arity 17: reference-member detection for the serializer backstop.
template <typename T>
constexpr auto has_reference_field_tag(T&, std::integral_constant<std::size_t, 17>)
    -> decltype([](T& u) {
           auto& [f0, f1, f2, f3, f4, f5, f6, f7, f8, f9, f10, f11, f12, f13, f14, f15, f16] = u;
           return std::bool_constant<(std::is_reference_v<decltype(f0)> || std::is_reference_v<decltype(f1)> || std::is_reference_v<decltype(f2)> || std::is_reference_v<decltype(f3)> || std::is_reference_v<decltype(f4)> || std::is_reference_v<decltype(f5)> || std::is_reference_v<decltype(f6)> || std::is_reference_v<decltype(f7)> || std::is_reference_v<decltype(f8)> || std::is_reference_v<decltype(f9)> || std::is_reference_v<decltype(f10)> || std::is_reference_v<decltype(f11)> || std::is_reference_v<decltype(f12)> || std::is_reference_v<decltype(f13)> || std::is_reference_v<decltype(f14)> || std::is_reference_v<decltype(f15)> || std::is_reference_v<decltype(f16)>)>{};
       }(std::declval<T&>()));

// Arity 18: decompose into a tuple of references.
template <typename T>
constexpr auto to_tuple_impl(T&& obj, std::integral_constant<std::size_t, 18>) {
    auto&& [f0, f1, f2, f3, f4, f5, f6, f7, f8, f9, f10, f11, f12, f13, f14, f15, f16, f17] = std::forward<T>(obj);
    return std::forward_as_tuple(f0, f1, f2, f3, f4, f5, f6, f7, f8, f9, f10, f11, f12, f13, f14, f15, f16, f17);
}

// Arity 18: reference-member detection for the serializer backstop.
template <typename T>
constexpr auto has_reference_field_tag(T&, std::integral_constant<std::size_t, 18>)
    -> decltype([](T& u) {
           auto& [f0, f1, f2, f3, f4, f5, f6, f7, f8, f9, f10, f11, f12, f13, f14, f15, f16, f17] = u;
           return std::bool_constant<(std::is_reference_v<decltype(f0)> || std::is_reference_v<decltype(f1)> || std::is_reference_v<decltype(f2)> || std::is_reference_v<decltype(f3)> || std::is_reference_v<decltype(f4)> || std::is_reference_v<decltype(f5)> || std::is_reference_v<decltype(f6)> || std::is_reference_v<decltype(f7)> || std::is_reference_v<decltype(f8)> || std::is_reference_v<decltype(f9)> || std::is_reference_v<decltype(f10)> || std::is_reference_v<decltype(f11)> || std::is_reference_v<decltype(f12)> || std::is_reference_v<decltype(f13)> || std::is_reference_v<decltype(f14)> || std::is_reference_v<decltype(f15)> || std::is_reference_v<decltype(f16)> || std::is_reference_v<decltype(f17)>)>{};
       }(std::declval<T&>()));

// Arity 19: decompose into a tuple of references.
template <typename T>
constexpr auto to_tuple_impl(T&& obj, std::integral_constant<std::size_t, 19>) {
    auto&& [f0, f1, f2, f3, f4, f5, f6, f7, f8, f9, f10, f11, f12, f13, f14, f15, f16, f17, f18] = std::forward<T>(obj);
    return std::forward_as_tuple(f0, f1, f2, f3, f4, f5, f6, f7, f8, f9, f10, f11, f12, f13, f14, f15, f16, f17, f18);
}

// Arity 19: reference-member detection for the serializer backstop.
template <typename T>
constexpr auto has_reference_field_tag(T&, std::integral_constant<std::size_t, 19>)
    -> decltype([](T& u) {
           auto& [f0, f1, f2, f3, f4, f5, f6, f7, f8, f9, f10, f11, f12, f13, f14, f15, f16, f17, f18] = u;
           return std::bool_constant<(std::is_reference_v<decltype(f0)> || std::is_reference_v<decltype(f1)> || std::is_reference_v<decltype(f2)> || std::is_reference_v<decltype(f3)> || std::is_reference_v<decltype(f4)> || std::is_reference_v<decltype(f5)> || std::is_reference_v<decltype(f6)> || std::is_reference_v<decltype(f7)> || std::is_reference_v<decltype(f8)> || std::is_reference_v<decltype(f9)> || std::is_reference_v<decltype(f10)> || std::is_reference_v<decltype(f11)> || std::is_reference_v<decltype(f12)> || std::is_reference_v<decltype(f13)> || std::is_reference_v<decltype(f14)> || std::is_reference_v<decltype(f15)> || std::is_reference_v<decltype(f16)> || std::is_reference_v<decltype(f17)> || std::is_reference_v<decltype(f18)>)>{};
       }(std::declval<T&>()));

// Arity 20: decompose into a tuple of references.
template <typename T>
constexpr auto to_tuple_impl(T&& obj, std::integral_constant<std::size_t, 20>) {
    auto&& [f0, f1, f2, f3, f4, f5, f6, f7, f8, f9, f10, f11, f12, f13, f14, f15, f16, f17, f18, f19] = std::forward<T>(obj);
    return std::forward_as_tuple(f0, f1, f2, f3, f4, f5, f6, f7, f8, f9, f10, f11, f12, f13, f14, f15, f16, f17, f18, f19);
}

// Arity 20: reference-member detection for the serializer backstop.
template <typename T>
constexpr auto has_reference_field_tag(T&, std::integral_constant<std::size_t, 20>)
    -> decltype([](T& u) {
           auto& [f0, f1, f2, f3, f4, f5, f6, f7, f8, f9, f10, f11, f12, f13, f14, f15, f16, f17, f18, f19] = u;
           return std::bool_constant<(std::is_reference_v<decltype(f0)> || std::is_reference_v<decltype(f1)> || std::is_reference_v<decltype(f2)> || std::is_reference_v<decltype(f3)> || std::is_reference_v<decltype(f4)> || std::is_reference_v<decltype(f5)> || std::is_reference_v<decltype(f6)> || std::is_reference_v<decltype(f7)> || std::is_reference_v<decltype(f8)> || std::is_reference_v<decltype(f9)> || std::is_reference_v<decltype(f10)> || std::is_reference_v<decltype(f11)> || std::is_reference_v<decltype(f12)> || std::is_reference_v<decltype(f13)> || std::is_reference_v<decltype(f14)> || std::is_reference_v<decltype(f15)> || std::is_reference_v<decltype(f16)> || std::is_reference_v<decltype(f17)> || std::is_reference_v<decltype(f18)> || std::is_reference_v<decltype(f19)>)>{};
       }(std::declval<T&>()));

// Arity 21: decompose into a tuple of references.
template <typename T>
constexpr auto to_tuple_impl(T&& obj, std::integral_constant<std::size_t, 21>) {
    auto&& [f0, f1, f2, f3, f4, f5, f6, f7, f8, f9, f10, f11, f12, f13, f14, f15, f16, f17, f18, f19, f20] = std::forward<T>(obj);
    return std::forward_as_tuple(f0, f1, f2, f3, f4, f5, f6, f7, f8, f9, f10, f11, f12, f13, f14, f15, f16, f17, f18, f19, f20);
}

// Arity 21: reference-member detection for the serializer backstop.
template <typename T>
constexpr auto has_reference_field_tag(T&, std::integral_constant<std::size_t, 21>)
    -> decltype([](T& u) {
           auto& [f0, f1, f2, f3, f4, f5, f6, f7, f8, f9, f10, f11, f12, f13, f14, f15, f16, f17, f18, f19, f20] = u;
           return std::bool_constant<(std::is_reference_v<decltype(f0)> || std::is_reference_v<decltype(f1)> || std::is_reference_v<decltype(f2)> || std::is_reference_v<decltype(f3)> || std::is_reference_v<decltype(f4)> || std::is_reference_v<decltype(f5)> || std::is_reference_v<decltype(f6)> || std::is_reference_v<decltype(f7)> || std::is_reference_v<decltype(f8)> || std::is_reference_v<decltype(f9)> || std::is_reference_v<decltype(f10)> || std::is_reference_v<decltype(f11)> || std::is_reference_v<decltype(f12)> || std::is_reference_v<decltype(f13)> || std::is_reference_v<decltype(f14)> || std::is_reference_v<decltype(f15)> || std::is_reference_v<decltype(f16)> || std::is_reference_v<decltype(f17)> || std::is_reference_v<decltype(f18)> || std::is_reference_v<decltype(f19)> || std::is_reference_v<decltype(f20)>)>{};
       }(std::declval<T&>()));

// Arity 22: decompose into a tuple of references.
template <typename T>
constexpr auto to_tuple_impl(T&& obj, std::integral_constant<std::size_t, 22>) {
    auto&& [f0, f1, f2, f3, f4, f5, f6, f7, f8, f9, f10, f11, f12, f13, f14, f15, f16, f17, f18, f19, f20, f21] = std::forward<T>(obj);
    return std::forward_as_tuple(f0, f1, f2, f3, f4, f5, f6, f7, f8, f9, f10, f11, f12, f13, f14, f15, f16, f17, f18, f19, f20, f21);
}

// Arity 22: reference-member detection for the serializer backstop.
template <typename T>
constexpr auto has_reference_field_tag(T&, std::integral_constant<std::size_t, 22>)
    -> decltype([](T& u) {
           auto& [f0, f1, f2, f3, f4, f5, f6, f7, f8, f9, f10, f11, f12, f13, f14, f15, f16, f17, f18, f19, f20, f21] = u;
           return std::bool_constant<(std::is_reference_v<decltype(f0)> || std::is_reference_v<decltype(f1)> || std::is_reference_v<decltype(f2)> || std::is_reference_v<decltype(f3)> || std::is_reference_v<decltype(f4)> || std::is_reference_v<decltype(f5)> || std::is_reference_v<decltype(f6)> || std::is_reference_v<decltype(f7)> || std::is_reference_v<decltype(f8)> || std::is_reference_v<decltype(f9)> || std::is_reference_v<decltype(f10)> || std::is_reference_v<decltype(f11)> || std::is_reference_v<decltype(f12)> || std::is_reference_v<decltype(f13)> || std::is_reference_v<decltype(f14)> || std::is_reference_v<decltype(f15)> || std::is_reference_v<decltype(f16)> || std::is_reference_v<decltype(f17)> || std::is_reference_v<decltype(f18)> || std::is_reference_v<decltype(f19)> || std::is_reference_v<decltype(f20)> || std::is_reference_v<decltype(f21)>)>{};
       }(std::declval<T&>()));

// Arity 23: decompose into a tuple of references.
template <typename T>
constexpr auto to_tuple_impl(T&& obj, std::integral_constant<std::size_t, 23>) {
    auto&& [f0, f1, f2, f3, f4, f5, f6, f7, f8, f9, f10, f11, f12, f13, f14, f15, f16, f17, f18, f19, f20, f21, f22] = std::forward<T>(obj);
    return std::forward_as_tuple(f0, f1, f2, f3, f4, f5, f6, f7, f8, f9, f10, f11, f12, f13, f14, f15, f16, f17, f18, f19, f20, f21, f22);
}

// Arity 23: reference-member detection for the serializer backstop.
template <typename T>
constexpr auto has_reference_field_tag(T&, std::integral_constant<std::size_t, 23>)
    -> decltype([](T& u) {
           auto& [f0, f1, f2, f3, f4, f5, f6, f7, f8, f9, f10, f11, f12, f13, f14, f15, f16, f17, f18, f19, f20, f21, f22] = u;
           return std::bool_constant<(std::is_reference_v<decltype(f0)> || std::is_reference_v<decltype(f1)> || std::is_reference_v<decltype(f2)> || std::is_reference_v<decltype(f3)> || std::is_reference_v<decltype(f4)> || std::is_reference_v<decltype(f5)> || std::is_reference_v<decltype(f6)> || std::is_reference_v<decltype(f7)> || std::is_reference_v<decltype(f8)> || std::is_reference_v<decltype(f9)> || std::is_reference_v<decltype(f10)> || std::is_reference_v<decltype(f11)> || std::is_reference_v<decltype(f12)> || std::is_reference_v<decltype(f13)> || std::is_reference_v<decltype(f14)> || std::is_reference_v<decltype(f15)> || std::is_reference_v<decltype(f16)> || std::is_reference_v<decltype(f17)> || std::is_reference_v<decltype(f18)> || std::is_reference_v<decltype(f19)> || std::is_reference_v<decltype(f20)> || std::is_reference_v<decltype(f21)> || std::is_reference_v<decltype(f22)>)>{};
       }(std::declval<T&>()));

// Arity 24: decompose into a tuple of references.
template <typename T>
constexpr auto to_tuple_impl(T&& obj, std::integral_constant<std::size_t, 24>) {
    auto&& [f0, f1, f2, f3, f4, f5, f6, f7, f8, f9, f10, f11, f12, f13, f14, f15, f16, f17, f18, f19, f20, f21, f22, f23] = std::forward<T>(obj);
    return std::forward_as_tuple(f0, f1, f2, f3, f4, f5, f6, f7, f8, f9, f10, f11, f12, f13, f14, f15, f16, f17, f18, f19, f20, f21, f22, f23);
}

// Arity 24: reference-member detection for the serializer backstop.
template <typename T>
constexpr auto has_reference_field_tag(T&, std::integral_constant<std::size_t, 24>)
    -> decltype([](T& u) {
           auto& [f0, f1, f2, f3, f4, f5, f6, f7, f8, f9, f10, f11, f12, f13, f14, f15, f16, f17, f18, f19, f20, f21, f22, f23] = u;
           return std::bool_constant<(std::is_reference_v<decltype(f0)> || std::is_reference_v<decltype(f1)> || std::is_reference_v<decltype(f2)> || std::is_reference_v<decltype(f3)> || std::is_reference_v<decltype(f4)> || std::is_reference_v<decltype(f5)> || std::is_reference_v<decltype(f6)> || std::is_reference_v<decltype(f7)> || std::is_reference_v<decltype(f8)> || std::is_reference_v<decltype(f9)> || std::is_reference_v<decltype(f10)> || std::is_reference_v<decltype(f11)> || std::is_reference_v<decltype(f12)> || std::is_reference_v<decltype(f13)> || std::is_reference_v<decltype(f14)> || std::is_reference_v<decltype(f15)> || std::is_reference_v<decltype(f16)> || std::is_reference_v<decltype(f17)> || std::is_reference_v<decltype(f18)> || std::is_reference_v<decltype(f19)> || std::is_reference_v<decltype(f20)> || std::is_reference_v<decltype(f21)> || std::is_reference_v<decltype(f22)> || std::is_reference_v<decltype(f23)>)>{};
       }(std::declval<T&>()));

// Arity 25: decompose into a tuple of references.
template <typename T>
constexpr auto to_tuple_impl(T&& obj, std::integral_constant<std::size_t, 25>) {
    auto&& [f0, f1, f2, f3, f4, f5, f6, f7, f8, f9, f10, f11, f12, f13, f14, f15, f16, f17, f18, f19, f20, f21, f22, f23, f24] = std::forward<T>(obj);
    return std::forward_as_tuple(f0, f1, f2, f3, f4, f5, f6, f7, f8, f9, f10, f11, f12, f13, f14, f15, f16, f17, f18, f19, f20, f21, f22, f23, f24);
}

// Arity 25: reference-member detection for the serializer backstop.
template <typename T>
constexpr auto has_reference_field_tag(T&, std::integral_constant<std::size_t, 25>)
    -> decltype([](T& u) {
           auto& [f0, f1, f2, f3, f4, f5, f6, f7, f8, f9, f10, f11, f12, f13, f14, f15, f16, f17, f18, f19, f20, f21, f22, f23, f24] = u;
           return std::bool_constant<(std::is_reference_v<decltype(f0)> || std::is_reference_v<decltype(f1)> || std::is_reference_v<decltype(f2)> || std::is_reference_v<decltype(f3)> || std::is_reference_v<decltype(f4)> || std::is_reference_v<decltype(f5)> || std::is_reference_v<decltype(f6)> || std::is_reference_v<decltype(f7)> || std::is_reference_v<decltype(f8)> || std::is_reference_v<decltype(f9)> || std::is_reference_v<decltype(f10)> || std::is_reference_v<decltype(f11)> || std::is_reference_v<decltype(f12)> || std::is_reference_v<decltype(f13)> || std::is_reference_v<decltype(f14)> || std::is_reference_v<decltype(f15)> || std::is_reference_v<decltype(f16)> || std::is_reference_v<decltype(f17)> || std::is_reference_v<decltype(f18)> || std::is_reference_v<decltype(f19)> || std::is_reference_v<decltype(f20)> || std::is_reference_v<decltype(f21)> || std::is_reference_v<decltype(f22)> || std::is_reference_v<decltype(f23)> || std::is_reference_v<decltype(f24)>)>{};
       }(std::declval<T&>()));

// Arity 26: decompose into a tuple of references.
template <typename T>
constexpr auto to_tuple_impl(T&& obj, std::integral_constant<std::size_t, 26>) {
    auto&& [f0, f1, f2, f3, f4, f5, f6, f7, f8, f9, f10, f11, f12, f13, f14, f15, f16, f17, f18, f19, f20, f21, f22, f23, f24, f25] = std::forward<T>(obj);
    return std::forward_as_tuple(f0, f1, f2, f3, f4, f5, f6, f7, f8, f9, f10, f11, f12, f13, f14, f15, f16, f17, f18, f19, f20, f21, f22, f23, f24, f25);
}

// Arity 26: reference-member detection for the serializer backstop.
template <typename T>
constexpr auto has_reference_field_tag(T&, std::integral_constant<std::size_t, 26>)
    -> decltype([](T& u) {
           auto& [f0, f1, f2, f3, f4, f5, f6, f7, f8, f9, f10, f11, f12, f13, f14, f15, f16, f17, f18, f19, f20, f21, f22, f23, f24, f25] = u;
           return std::bool_constant<(std::is_reference_v<decltype(f0)> || std::is_reference_v<decltype(f1)> || std::is_reference_v<decltype(f2)> || std::is_reference_v<decltype(f3)> || std::is_reference_v<decltype(f4)> || std::is_reference_v<decltype(f5)> || std::is_reference_v<decltype(f6)> || std::is_reference_v<decltype(f7)> || std::is_reference_v<decltype(f8)> || std::is_reference_v<decltype(f9)> || std::is_reference_v<decltype(f10)> || std::is_reference_v<decltype(f11)> || std::is_reference_v<decltype(f12)> || std::is_reference_v<decltype(f13)> || std::is_reference_v<decltype(f14)> || std::is_reference_v<decltype(f15)> || std::is_reference_v<decltype(f16)> || std::is_reference_v<decltype(f17)> || std::is_reference_v<decltype(f18)> || std::is_reference_v<decltype(f19)> || std::is_reference_v<decltype(f20)> || std::is_reference_v<decltype(f21)> || std::is_reference_v<decltype(f22)> || std::is_reference_v<decltype(f23)> || std::is_reference_v<decltype(f24)> || std::is_reference_v<decltype(f25)>)>{};
       }(std::declval<T&>()));

// Arity 27: decompose into a tuple of references.
template <typename T>
constexpr auto to_tuple_impl(T&& obj, std::integral_constant<std::size_t, 27>) {
    auto&& [f0, f1, f2, f3, f4, f5, f6, f7, f8, f9, f10, f11, f12, f13, f14, f15, f16, f17, f18, f19, f20, f21, f22, f23, f24, f25, f26] = std::forward<T>(obj);
    return std::forward_as_tuple(f0, f1, f2, f3, f4, f5, f6, f7, f8, f9, f10, f11, f12, f13, f14, f15, f16, f17, f18, f19, f20, f21, f22, f23, f24, f25, f26);
}

// Arity 27: reference-member detection for the serializer backstop.
template <typename T>
constexpr auto has_reference_field_tag(T&, std::integral_constant<std::size_t, 27>)
    -> decltype([](T& u) {
           auto& [f0, f1, f2, f3, f4, f5, f6, f7, f8, f9, f10, f11, f12, f13, f14, f15, f16, f17, f18, f19, f20, f21, f22, f23, f24, f25, f26] = u;
           return std::bool_constant<(std::is_reference_v<decltype(f0)> || std::is_reference_v<decltype(f1)> || std::is_reference_v<decltype(f2)> || std::is_reference_v<decltype(f3)> || std::is_reference_v<decltype(f4)> || std::is_reference_v<decltype(f5)> || std::is_reference_v<decltype(f6)> || std::is_reference_v<decltype(f7)> || std::is_reference_v<decltype(f8)> || std::is_reference_v<decltype(f9)> || std::is_reference_v<decltype(f10)> || std::is_reference_v<decltype(f11)> || std::is_reference_v<decltype(f12)> || std::is_reference_v<decltype(f13)> || std::is_reference_v<decltype(f14)> || std::is_reference_v<decltype(f15)> || std::is_reference_v<decltype(f16)> || std::is_reference_v<decltype(f17)> || std::is_reference_v<decltype(f18)> || std::is_reference_v<decltype(f19)> || std::is_reference_v<decltype(f20)> || std::is_reference_v<decltype(f21)> || std::is_reference_v<decltype(f22)> || std::is_reference_v<decltype(f23)> || std::is_reference_v<decltype(f24)> || std::is_reference_v<decltype(f25)> || std::is_reference_v<decltype(f26)>)>{};
       }(std::declval<T&>()));

// Arity 28: decompose into a tuple of references.
template <typename T>
constexpr auto to_tuple_impl(T&& obj, std::integral_constant<std::size_t, 28>) {
    auto&& [f0, f1, f2, f3, f4, f5, f6, f7, f8, f9, f10, f11, f12, f13, f14, f15, f16, f17, f18, f19, f20, f21, f22, f23, f24, f25, f26, f27] = std::forward<T>(obj);
    return std::forward_as_tuple(f0, f1, f2, f3, f4, f5, f6, f7, f8, f9, f10, f11, f12, f13, f14, f15, f16, f17, f18, f19, f20, f21, f22, f23, f24, f25, f26, f27);
}

// Arity 28: reference-member detection for the serializer backstop.
template <typename T>
constexpr auto has_reference_field_tag(T&, std::integral_constant<std::size_t, 28>)
    -> decltype([](T& u) {
           auto& [f0, f1, f2, f3, f4, f5, f6, f7, f8, f9, f10, f11, f12, f13, f14, f15, f16, f17, f18, f19, f20, f21, f22, f23, f24, f25, f26, f27] = u;
           return std::bool_constant<(std::is_reference_v<decltype(f0)> || std::is_reference_v<decltype(f1)> || std::is_reference_v<decltype(f2)> || std::is_reference_v<decltype(f3)> || std::is_reference_v<decltype(f4)> || std::is_reference_v<decltype(f5)> || std::is_reference_v<decltype(f6)> || std::is_reference_v<decltype(f7)> || std::is_reference_v<decltype(f8)> || std::is_reference_v<decltype(f9)> || std::is_reference_v<decltype(f10)> || std::is_reference_v<decltype(f11)> || std::is_reference_v<decltype(f12)> || std::is_reference_v<decltype(f13)> || std::is_reference_v<decltype(f14)> || std::is_reference_v<decltype(f15)> || std::is_reference_v<decltype(f16)> || std::is_reference_v<decltype(f17)> || std::is_reference_v<decltype(f18)> || std::is_reference_v<decltype(f19)> || std::is_reference_v<decltype(f20)> || std::is_reference_v<decltype(f21)> || std::is_reference_v<decltype(f22)> || std::is_reference_v<decltype(f23)> || std::is_reference_v<decltype(f24)> || std::is_reference_v<decltype(f25)> || std::is_reference_v<decltype(f26)> || std::is_reference_v<decltype(f27)>)>{};
       }(std::declval<T&>()));

// Arity 29: decompose into a tuple of references.
template <typename T>
constexpr auto to_tuple_impl(T&& obj, std::integral_constant<std::size_t, 29>) {
    auto&& [f0, f1, f2, f3, f4, f5, f6, f7, f8, f9, f10, f11, f12, f13, f14, f15, f16, f17, f18, f19, f20, f21, f22, f23, f24, f25, f26, f27, f28] = std::forward<T>(obj);
    return std::forward_as_tuple(f0, f1, f2, f3, f4, f5, f6, f7, f8, f9, f10, f11, f12, f13, f14, f15, f16, f17, f18, f19, f20, f21, f22, f23, f24, f25, f26, f27, f28);
}

// Arity 29: reference-member detection for the serializer backstop.
template <typename T>
constexpr auto has_reference_field_tag(T&, std::integral_constant<std::size_t, 29>)
    -> decltype([](T& u) {
           auto& [f0, f1, f2, f3, f4, f5, f6, f7, f8, f9, f10, f11, f12, f13, f14, f15, f16, f17, f18, f19, f20, f21, f22, f23, f24, f25, f26, f27, f28] = u;
           return std::bool_constant<(std::is_reference_v<decltype(f0)> || std::is_reference_v<decltype(f1)> || std::is_reference_v<decltype(f2)> || std::is_reference_v<decltype(f3)> || std::is_reference_v<decltype(f4)> || std::is_reference_v<decltype(f5)> || std::is_reference_v<decltype(f6)> || std::is_reference_v<decltype(f7)> || std::is_reference_v<decltype(f8)> || std::is_reference_v<decltype(f9)> || std::is_reference_v<decltype(f10)> || std::is_reference_v<decltype(f11)> || std::is_reference_v<decltype(f12)> || std::is_reference_v<decltype(f13)> || std::is_reference_v<decltype(f14)> || std::is_reference_v<decltype(f15)> || std::is_reference_v<decltype(f16)> || std::is_reference_v<decltype(f17)> || std::is_reference_v<decltype(f18)> || std::is_reference_v<decltype(f19)> || std::is_reference_v<decltype(f20)> || std::is_reference_v<decltype(f21)> || std::is_reference_v<decltype(f22)> || std::is_reference_v<decltype(f23)> || std::is_reference_v<decltype(f24)> || std::is_reference_v<decltype(f25)> || std::is_reference_v<decltype(f26)> || std::is_reference_v<decltype(f27)> || std::is_reference_v<decltype(f28)>)>{};
       }(std::declval<T&>()));

// Arity 30: decompose into a tuple of references.
template <typename T>
constexpr auto to_tuple_impl(T&& obj, std::integral_constant<std::size_t, 30>) {
    auto&& [f0, f1, f2, f3, f4, f5, f6, f7, f8, f9, f10, f11, f12, f13, f14, f15, f16, f17, f18, f19, f20, f21, f22, f23, f24, f25, f26, f27, f28, f29] = std::forward<T>(obj);
    return std::forward_as_tuple(f0, f1, f2, f3, f4, f5, f6, f7, f8, f9, f10, f11, f12, f13, f14, f15, f16, f17, f18, f19, f20, f21, f22, f23, f24, f25, f26, f27, f28, f29);
}

// Arity 30: reference-member detection for the serializer backstop.
template <typename T>
constexpr auto has_reference_field_tag(T&, std::integral_constant<std::size_t, 30>)
    -> decltype([](T& u) {
           auto& [f0, f1, f2, f3, f4, f5, f6, f7, f8, f9, f10, f11, f12, f13, f14, f15, f16, f17, f18, f19, f20, f21, f22, f23, f24, f25, f26, f27, f28, f29] = u;
           return std::bool_constant<(std::is_reference_v<decltype(f0)> || std::is_reference_v<decltype(f1)> || std::is_reference_v<decltype(f2)> || std::is_reference_v<decltype(f3)> || std::is_reference_v<decltype(f4)> || std::is_reference_v<decltype(f5)> || std::is_reference_v<decltype(f6)> || std::is_reference_v<decltype(f7)> || std::is_reference_v<decltype(f8)> || std::is_reference_v<decltype(f9)> || std::is_reference_v<decltype(f10)> || std::is_reference_v<decltype(f11)> || std::is_reference_v<decltype(f12)> || std::is_reference_v<decltype(f13)> || std::is_reference_v<decltype(f14)> || std::is_reference_v<decltype(f15)> || std::is_reference_v<decltype(f16)> || std::is_reference_v<decltype(f17)> || std::is_reference_v<decltype(f18)> || std::is_reference_v<decltype(f19)> || std::is_reference_v<decltype(f20)> || std::is_reference_v<decltype(f21)> || std::is_reference_v<decltype(f22)> || std::is_reference_v<decltype(f23)> || std::is_reference_v<decltype(f24)> || std::is_reference_v<decltype(f25)> || std::is_reference_v<decltype(f26)> || std::is_reference_v<decltype(f27)> || std::is_reference_v<decltype(f28)> || std::is_reference_v<decltype(f29)>)>{};
       }(std::declval<T&>()));

// Arity 31: decompose into a tuple of references.
template <typename T>
constexpr auto to_tuple_impl(T&& obj, std::integral_constant<std::size_t, 31>) {
    auto&& [f0, f1, f2, f3, f4, f5, f6, f7, f8, f9, f10, f11, f12, f13, f14, f15, f16, f17, f18, f19, f20, f21, f22, f23, f24, f25, f26, f27, f28, f29, f30] = std::forward<T>(obj);
    return std::forward_as_tuple(f0, f1, f2, f3, f4, f5, f6, f7, f8, f9, f10, f11, f12, f13, f14, f15, f16, f17, f18, f19, f20, f21, f22, f23, f24, f25, f26, f27, f28, f29, f30);
}

// Arity 31: reference-member detection for the serializer backstop.
template <typename T>
constexpr auto has_reference_field_tag(T&, std::integral_constant<std::size_t, 31>)
    -> decltype([](T& u) {
           auto& [f0, f1, f2, f3, f4, f5, f6, f7, f8, f9, f10, f11, f12, f13, f14, f15, f16, f17, f18, f19, f20, f21, f22, f23, f24, f25, f26, f27, f28, f29, f30] = u;
           return std::bool_constant<(std::is_reference_v<decltype(f0)> || std::is_reference_v<decltype(f1)> || std::is_reference_v<decltype(f2)> || std::is_reference_v<decltype(f3)> || std::is_reference_v<decltype(f4)> || std::is_reference_v<decltype(f5)> || std::is_reference_v<decltype(f6)> || std::is_reference_v<decltype(f7)> || std::is_reference_v<decltype(f8)> || std::is_reference_v<decltype(f9)> || std::is_reference_v<decltype(f10)> || std::is_reference_v<decltype(f11)> || std::is_reference_v<decltype(f12)> || std::is_reference_v<decltype(f13)> || std::is_reference_v<decltype(f14)> || std::is_reference_v<decltype(f15)> || std::is_reference_v<decltype(f16)> || std::is_reference_v<decltype(f17)> || std::is_reference_v<decltype(f18)> || std::is_reference_v<decltype(f19)> || std::is_reference_v<decltype(f20)> || std::is_reference_v<decltype(f21)> || std::is_reference_v<decltype(f22)> || std::is_reference_v<decltype(f23)> || std::is_reference_v<decltype(f24)> || std::is_reference_v<decltype(f25)> || std::is_reference_v<decltype(f26)> || std::is_reference_v<decltype(f27)> || std::is_reference_v<decltype(f28)> || std::is_reference_v<decltype(f29)> || std::is_reference_v<decltype(f30)>)>{};
       }(std::declval<T&>()));

// Arity 32: decompose into a tuple of references.
template <typename T>
constexpr auto to_tuple_impl(T&& obj, std::integral_constant<std::size_t, 32>) {
    auto&& [f0, f1, f2, f3, f4, f5, f6, f7, f8, f9, f10, f11, f12, f13, f14, f15, f16, f17, f18, f19, f20, f21, f22, f23, f24, f25, f26, f27, f28, f29, f30, f31] = std::forward<T>(obj);
    return std::forward_as_tuple(f0, f1, f2, f3, f4, f5, f6, f7, f8, f9, f10, f11, f12, f13, f14, f15, f16, f17, f18, f19, f20, f21, f22, f23, f24, f25, f26, f27, f28, f29, f30, f31);
}

// Arity 32: reference-member detection for the serializer backstop.
template <typename T>
constexpr auto has_reference_field_tag(T&, std::integral_constant<std::size_t, 32>)
    -> decltype([](T& u) {
           auto& [f0, f1, f2, f3, f4, f5, f6, f7, f8, f9, f10, f11, f12, f13, f14, f15, f16, f17, f18, f19, f20, f21, f22, f23, f24, f25, f26, f27, f28, f29, f30, f31] = u;
           return std::bool_constant<(std::is_reference_v<decltype(f0)> || std::is_reference_v<decltype(f1)> || std::is_reference_v<decltype(f2)> || std::is_reference_v<decltype(f3)> || std::is_reference_v<decltype(f4)> || std::is_reference_v<decltype(f5)> || std::is_reference_v<decltype(f6)> || std::is_reference_v<decltype(f7)> || std::is_reference_v<decltype(f8)> || std::is_reference_v<decltype(f9)> || std::is_reference_v<decltype(f10)> || std::is_reference_v<decltype(f11)> || std::is_reference_v<decltype(f12)> || std::is_reference_v<decltype(f13)> || std::is_reference_v<decltype(f14)> || std::is_reference_v<decltype(f15)> || std::is_reference_v<decltype(f16)> || std::is_reference_v<decltype(f17)> || std::is_reference_v<decltype(f18)> || std::is_reference_v<decltype(f19)> || std::is_reference_v<decltype(f20)> || std::is_reference_v<decltype(f21)> || std::is_reference_v<decltype(f22)> || std::is_reference_v<decltype(f23)> || std::is_reference_v<decltype(f24)> || std::is_reference_v<decltype(f25)> || std::is_reference_v<decltype(f26)> || std::is_reference_v<decltype(f27)> || std::is_reference_v<decltype(f28)> || std::is_reference_v<decltype(f29)> || std::is_reference_v<decltype(f30)> || std::is_reference_v<decltype(f31)>)>{};
       }(std::declval<T&>()));

// Arity 33: decompose into a tuple of references.
template <typename T>
constexpr auto to_tuple_impl(T&& obj, std::integral_constant<std::size_t, 33>) {
    auto&& [f0, f1, f2, f3, f4, f5, f6, f7, f8, f9, f10, f11, f12, f13, f14, f15, f16, f17, f18, f19, f20, f21, f22, f23, f24, f25, f26, f27, f28, f29, f30, f31, f32] = std::forward<T>(obj);
    return std::forward_as_tuple(f0, f1, f2, f3, f4, f5, f6, f7, f8, f9, f10, f11, f12, f13, f14, f15, f16, f17, f18, f19, f20, f21, f22, f23, f24, f25, f26, f27, f28, f29, f30, f31, f32);
}

// Arity 33: reference-member detection for the serializer backstop.
template <typename T>
constexpr auto has_reference_field_tag(T&, std::integral_constant<std::size_t, 33>)
    -> decltype([](T& u) {
           auto& [f0, f1, f2, f3, f4, f5, f6, f7, f8, f9, f10, f11, f12, f13, f14, f15, f16, f17, f18, f19, f20, f21, f22, f23, f24, f25, f26, f27, f28, f29, f30, f31, f32] = u;
           return std::bool_constant<(std::is_reference_v<decltype(f0)> || std::is_reference_v<decltype(f1)> || std::is_reference_v<decltype(f2)> || std::is_reference_v<decltype(f3)> || std::is_reference_v<decltype(f4)> || std::is_reference_v<decltype(f5)> || std::is_reference_v<decltype(f6)> || std::is_reference_v<decltype(f7)> || std::is_reference_v<decltype(f8)> || std::is_reference_v<decltype(f9)> || std::is_reference_v<decltype(f10)> || std::is_reference_v<decltype(f11)> || std::is_reference_v<decltype(f12)> || std::is_reference_v<decltype(f13)> || std::is_reference_v<decltype(f14)> || std::is_reference_v<decltype(f15)> || std::is_reference_v<decltype(f16)> || std::is_reference_v<decltype(f17)> || std::is_reference_v<decltype(f18)> || std::is_reference_v<decltype(f19)> || std::is_reference_v<decltype(f20)> || std::is_reference_v<decltype(f21)> || std::is_reference_v<decltype(f22)> || std::is_reference_v<decltype(f23)> || std::is_reference_v<decltype(f24)> || std::is_reference_v<decltype(f25)> || std::is_reference_v<decltype(f26)> || std::is_reference_v<decltype(f27)> || std::is_reference_v<decltype(f28)> || std::is_reference_v<decltype(f29)> || std::is_reference_v<decltype(f30)> || std::is_reference_v<decltype(f31)> || std::is_reference_v<decltype(f32)>)>{};
       }(std::declval<T&>()));

// Arity 34: decompose into a tuple of references.
template <typename T>
constexpr auto to_tuple_impl(T&& obj, std::integral_constant<std::size_t, 34>) {
    auto&& [f0, f1, f2, f3, f4, f5, f6, f7, f8, f9, f10, f11, f12, f13, f14, f15, f16, f17, f18, f19, f20, f21, f22, f23, f24, f25, f26, f27, f28, f29, f30, f31, f32, f33] = std::forward<T>(obj);
    return std::forward_as_tuple(f0, f1, f2, f3, f4, f5, f6, f7, f8, f9, f10, f11, f12, f13, f14, f15, f16, f17, f18, f19, f20, f21, f22, f23, f24, f25, f26, f27, f28, f29, f30, f31, f32, f33);
}

// Arity 34: reference-member detection for the serializer backstop.
template <typename T>
constexpr auto has_reference_field_tag(T&, std::integral_constant<std::size_t, 34>)
    -> decltype([](T& u) {
           auto& [f0, f1, f2, f3, f4, f5, f6, f7, f8, f9, f10, f11, f12, f13, f14, f15, f16, f17, f18, f19, f20, f21, f22, f23, f24, f25, f26, f27, f28, f29, f30, f31, f32, f33] = u;
           return std::bool_constant<(std::is_reference_v<decltype(f0)> || std::is_reference_v<decltype(f1)> || std::is_reference_v<decltype(f2)> || std::is_reference_v<decltype(f3)> || std::is_reference_v<decltype(f4)> || std::is_reference_v<decltype(f5)> || std::is_reference_v<decltype(f6)> || std::is_reference_v<decltype(f7)> || std::is_reference_v<decltype(f8)> || std::is_reference_v<decltype(f9)> || std::is_reference_v<decltype(f10)> || std::is_reference_v<decltype(f11)> || std::is_reference_v<decltype(f12)> || std::is_reference_v<decltype(f13)> || std::is_reference_v<decltype(f14)> || std::is_reference_v<decltype(f15)> || std::is_reference_v<decltype(f16)> || std::is_reference_v<decltype(f17)> || std::is_reference_v<decltype(f18)> || std::is_reference_v<decltype(f19)> || std::is_reference_v<decltype(f20)> || std::is_reference_v<decltype(f21)> || std::is_reference_v<decltype(f22)> || std::is_reference_v<decltype(f23)> || std::is_reference_v<decltype(f24)> || std::is_reference_v<decltype(f25)> || std::is_reference_v<decltype(f26)> || std::is_reference_v<decltype(f27)> || std::is_reference_v<decltype(f28)> || std::is_reference_v<decltype(f29)> || std::is_reference_v<decltype(f30)> || std::is_reference_v<decltype(f31)> || std::is_reference_v<decltype(f32)> || std::is_reference_v<decltype(f33)>)>{};
       }(std::declval<T&>()));

// Arity 35: decompose into a tuple of references.
template <typename T>
constexpr auto to_tuple_impl(T&& obj, std::integral_constant<std::size_t, 35>) {
    auto&& [f0, f1, f2, f3, f4, f5, f6, f7, f8, f9, f10, f11, f12, f13, f14, f15, f16, f17, f18, f19, f20, f21, f22, f23, f24, f25, f26, f27, f28, f29, f30, f31, f32, f33, f34] = std::forward<T>(obj);
    return std::forward_as_tuple(f0, f1, f2, f3, f4, f5, f6, f7, f8, f9, f10, f11, f12, f13, f14, f15, f16, f17, f18, f19, f20, f21, f22, f23, f24, f25, f26, f27, f28, f29, f30, f31, f32, f33, f34);
}

// Arity 35: reference-member detection for the serializer backstop.
template <typename T>
constexpr auto has_reference_field_tag(T&, std::integral_constant<std::size_t, 35>)
    -> decltype([](T& u) {
           auto& [f0, f1, f2, f3, f4, f5, f6, f7, f8, f9, f10, f11, f12, f13, f14, f15, f16, f17, f18, f19, f20, f21, f22, f23, f24, f25, f26, f27, f28, f29, f30, f31, f32, f33, f34] = u;
           return std::bool_constant<(std::is_reference_v<decltype(f0)> || std::is_reference_v<decltype(f1)> || std::is_reference_v<decltype(f2)> || std::is_reference_v<decltype(f3)> || std::is_reference_v<decltype(f4)> || std::is_reference_v<decltype(f5)> || std::is_reference_v<decltype(f6)> || std::is_reference_v<decltype(f7)> || std::is_reference_v<decltype(f8)> || std::is_reference_v<decltype(f9)> || std::is_reference_v<decltype(f10)> || std::is_reference_v<decltype(f11)> || std::is_reference_v<decltype(f12)> || std::is_reference_v<decltype(f13)> || std::is_reference_v<decltype(f14)> || std::is_reference_v<decltype(f15)> || std::is_reference_v<decltype(f16)> || std::is_reference_v<decltype(f17)> || std::is_reference_v<decltype(f18)> || std::is_reference_v<decltype(f19)> || std::is_reference_v<decltype(f20)> || std::is_reference_v<decltype(f21)> || std::is_reference_v<decltype(f22)> || std::is_reference_v<decltype(f23)> || std::is_reference_v<decltype(f24)> || std::is_reference_v<decltype(f25)> || std::is_reference_v<decltype(f26)> || std::is_reference_v<decltype(f27)> || std::is_reference_v<decltype(f28)> || std::is_reference_v<decltype(f29)> || std::is_reference_v<decltype(f30)> || std::is_reference_v<decltype(f31)> || std::is_reference_v<decltype(f32)> || std::is_reference_v<decltype(f33)> || std::is_reference_v<decltype(f34)>)>{};
       }(std::declval<T&>()));

// Arity 36: decompose into a tuple of references.
template <typename T>
constexpr auto to_tuple_impl(T&& obj, std::integral_constant<std::size_t, 36>) {
    auto&& [f0, f1, f2, f3, f4, f5, f6, f7, f8, f9, f10, f11, f12, f13, f14, f15, f16, f17, f18, f19, f20, f21, f22, f23, f24, f25, f26, f27, f28, f29, f30, f31, f32, f33, f34, f35] = std::forward<T>(obj);
    return std::forward_as_tuple(f0, f1, f2, f3, f4, f5, f6, f7, f8, f9, f10, f11, f12, f13, f14, f15, f16, f17, f18, f19, f20, f21, f22, f23, f24, f25, f26, f27, f28, f29, f30, f31, f32, f33, f34, f35);
}

// Arity 36: reference-member detection for the serializer backstop.
template <typename T>
constexpr auto has_reference_field_tag(T&, std::integral_constant<std::size_t, 36>)
    -> decltype([](T& u) {
           auto& [f0, f1, f2, f3, f4, f5, f6, f7, f8, f9, f10, f11, f12, f13, f14, f15, f16, f17, f18, f19, f20, f21, f22, f23, f24, f25, f26, f27, f28, f29, f30, f31, f32, f33, f34, f35] = u;
           return std::bool_constant<(std::is_reference_v<decltype(f0)> || std::is_reference_v<decltype(f1)> || std::is_reference_v<decltype(f2)> || std::is_reference_v<decltype(f3)> || std::is_reference_v<decltype(f4)> || std::is_reference_v<decltype(f5)> || std::is_reference_v<decltype(f6)> || std::is_reference_v<decltype(f7)> || std::is_reference_v<decltype(f8)> || std::is_reference_v<decltype(f9)> || std::is_reference_v<decltype(f10)> || std::is_reference_v<decltype(f11)> || std::is_reference_v<decltype(f12)> || std::is_reference_v<decltype(f13)> || std::is_reference_v<decltype(f14)> || std::is_reference_v<decltype(f15)> || std::is_reference_v<decltype(f16)> || std::is_reference_v<decltype(f17)> || std::is_reference_v<decltype(f18)> || std::is_reference_v<decltype(f19)> || std::is_reference_v<decltype(f20)> || std::is_reference_v<decltype(f21)> || std::is_reference_v<decltype(f22)> || std::is_reference_v<decltype(f23)> || std::is_reference_v<decltype(f24)> || std::is_reference_v<decltype(f25)> || std::is_reference_v<decltype(f26)> || std::is_reference_v<decltype(f27)> || std::is_reference_v<decltype(f28)> || std::is_reference_v<decltype(f29)> || std::is_reference_v<decltype(f30)> || std::is_reference_v<decltype(f31)> || std::is_reference_v<decltype(f32)> || std::is_reference_v<decltype(f33)> || std::is_reference_v<decltype(f34)> || std::is_reference_v<decltype(f35)>)>{};
       }(std::declval<T&>()));

// Arity 37: decompose into a tuple of references.
template <typename T>
constexpr auto to_tuple_impl(T&& obj, std::integral_constant<std::size_t, 37>) {
    auto&& [f0, f1, f2, f3, f4, f5, f6, f7, f8, f9, f10, f11, f12, f13, f14, f15, f16, f17, f18, f19, f20, f21, f22, f23, f24, f25, f26, f27, f28, f29, f30, f31, f32, f33, f34, f35, f36] = std::forward<T>(obj);
    return std::forward_as_tuple(f0, f1, f2, f3, f4, f5, f6, f7, f8, f9, f10, f11, f12, f13, f14, f15, f16, f17, f18, f19, f20, f21, f22, f23, f24, f25, f26, f27, f28, f29, f30, f31, f32, f33, f34, f35, f36);
}

// Arity 37: reference-member detection for the serializer backstop.
template <typename T>
constexpr auto has_reference_field_tag(T&, std::integral_constant<std::size_t, 37>)
    -> decltype([](T& u) {
           auto& [f0, f1, f2, f3, f4, f5, f6, f7, f8, f9, f10, f11, f12, f13, f14, f15, f16, f17, f18, f19, f20, f21, f22, f23, f24, f25, f26, f27, f28, f29, f30, f31, f32, f33, f34, f35, f36] = u;
           return std::bool_constant<(std::is_reference_v<decltype(f0)> || std::is_reference_v<decltype(f1)> || std::is_reference_v<decltype(f2)> || std::is_reference_v<decltype(f3)> || std::is_reference_v<decltype(f4)> || std::is_reference_v<decltype(f5)> || std::is_reference_v<decltype(f6)> || std::is_reference_v<decltype(f7)> || std::is_reference_v<decltype(f8)> || std::is_reference_v<decltype(f9)> || std::is_reference_v<decltype(f10)> || std::is_reference_v<decltype(f11)> || std::is_reference_v<decltype(f12)> || std::is_reference_v<decltype(f13)> || std::is_reference_v<decltype(f14)> || std::is_reference_v<decltype(f15)> || std::is_reference_v<decltype(f16)> || std::is_reference_v<decltype(f17)> || std::is_reference_v<decltype(f18)> || std::is_reference_v<decltype(f19)> || std::is_reference_v<decltype(f20)> || std::is_reference_v<decltype(f21)> || std::is_reference_v<decltype(f22)> || std::is_reference_v<decltype(f23)> || std::is_reference_v<decltype(f24)> || std::is_reference_v<decltype(f25)> || std::is_reference_v<decltype(f26)> || std::is_reference_v<decltype(f27)> || std::is_reference_v<decltype(f28)> || std::is_reference_v<decltype(f29)> || std::is_reference_v<decltype(f30)> || std::is_reference_v<decltype(f31)> || std::is_reference_v<decltype(f32)> || std::is_reference_v<decltype(f33)> || std::is_reference_v<decltype(f34)> || std::is_reference_v<decltype(f35)> || std::is_reference_v<decltype(f36)>)>{};
       }(std::declval<T&>()));

// Arity 38: decompose into a tuple of references.
template <typename T>
constexpr auto to_tuple_impl(T&& obj, std::integral_constant<std::size_t, 38>) {
    auto&& [f0, f1, f2, f3, f4, f5, f6, f7, f8, f9, f10, f11, f12, f13, f14, f15, f16, f17, f18, f19, f20, f21, f22, f23, f24, f25, f26, f27, f28, f29, f30, f31, f32, f33, f34, f35, f36, f37] = std::forward<T>(obj);
    return std::forward_as_tuple(f0, f1, f2, f3, f4, f5, f6, f7, f8, f9, f10, f11, f12, f13, f14, f15, f16, f17, f18, f19, f20, f21, f22, f23, f24, f25, f26, f27, f28, f29, f30, f31, f32, f33, f34, f35, f36, f37);
}

// Arity 38: reference-member detection for the serializer backstop.
template <typename T>
constexpr auto has_reference_field_tag(T&, std::integral_constant<std::size_t, 38>)
    -> decltype([](T& u) {
           auto& [f0, f1, f2, f3, f4, f5, f6, f7, f8, f9, f10, f11, f12, f13, f14, f15, f16, f17, f18, f19, f20, f21, f22, f23, f24, f25, f26, f27, f28, f29, f30, f31, f32, f33, f34, f35, f36, f37] = u;
           return std::bool_constant<(std::is_reference_v<decltype(f0)> || std::is_reference_v<decltype(f1)> || std::is_reference_v<decltype(f2)> || std::is_reference_v<decltype(f3)> || std::is_reference_v<decltype(f4)> || std::is_reference_v<decltype(f5)> || std::is_reference_v<decltype(f6)> || std::is_reference_v<decltype(f7)> || std::is_reference_v<decltype(f8)> || std::is_reference_v<decltype(f9)> || std::is_reference_v<decltype(f10)> || std::is_reference_v<decltype(f11)> || std::is_reference_v<decltype(f12)> || std::is_reference_v<decltype(f13)> || std::is_reference_v<decltype(f14)> || std::is_reference_v<decltype(f15)> || std::is_reference_v<decltype(f16)> || std::is_reference_v<decltype(f17)> || std::is_reference_v<decltype(f18)> || std::is_reference_v<decltype(f19)> || std::is_reference_v<decltype(f20)> || std::is_reference_v<decltype(f21)> || std::is_reference_v<decltype(f22)> || std::is_reference_v<decltype(f23)> || std::is_reference_v<decltype(f24)> || std::is_reference_v<decltype(f25)> || std::is_reference_v<decltype(f26)> || std::is_reference_v<decltype(f27)> || std::is_reference_v<decltype(f28)> || std::is_reference_v<decltype(f29)> || std::is_reference_v<decltype(f30)> || std::is_reference_v<decltype(f31)> || std::is_reference_v<decltype(f32)> || std::is_reference_v<decltype(f33)> || std::is_reference_v<decltype(f34)> || std::is_reference_v<decltype(f35)> || std::is_reference_v<decltype(f36)> || std::is_reference_v<decltype(f37)>)>{};
       }(std::declval<T&>()));

// Arity 39: decompose into a tuple of references.
template <typename T>
constexpr auto to_tuple_impl(T&& obj, std::integral_constant<std::size_t, 39>) {
    auto&& [f0, f1, f2, f3, f4, f5, f6, f7, f8, f9, f10, f11, f12, f13, f14, f15, f16, f17, f18, f19, f20, f21, f22, f23, f24, f25, f26, f27, f28, f29, f30, f31, f32, f33, f34, f35, f36, f37, f38] = std::forward<T>(obj);
    return std::forward_as_tuple(f0, f1, f2, f3, f4, f5, f6, f7, f8, f9, f10, f11, f12, f13, f14, f15, f16, f17, f18, f19, f20, f21, f22, f23, f24, f25, f26, f27, f28, f29, f30, f31, f32, f33, f34, f35, f36, f37, f38);
}

// Arity 39: reference-member detection for the serializer backstop.
template <typename T>
constexpr auto has_reference_field_tag(T&, std::integral_constant<std::size_t, 39>)
    -> decltype([](T& u) {
           auto& [f0, f1, f2, f3, f4, f5, f6, f7, f8, f9, f10, f11, f12, f13, f14, f15, f16, f17, f18, f19, f20, f21, f22, f23, f24, f25, f26, f27, f28, f29, f30, f31, f32, f33, f34, f35, f36, f37, f38] = u;
           return std::bool_constant<(std::is_reference_v<decltype(f0)> || std::is_reference_v<decltype(f1)> || std::is_reference_v<decltype(f2)> || std::is_reference_v<decltype(f3)> || std::is_reference_v<decltype(f4)> || std::is_reference_v<decltype(f5)> || std::is_reference_v<decltype(f6)> || std::is_reference_v<decltype(f7)> || std::is_reference_v<decltype(f8)> || std::is_reference_v<decltype(f9)> || std::is_reference_v<decltype(f10)> || std::is_reference_v<decltype(f11)> || std::is_reference_v<decltype(f12)> || std::is_reference_v<decltype(f13)> || std::is_reference_v<decltype(f14)> || std::is_reference_v<decltype(f15)> || std::is_reference_v<decltype(f16)> || std::is_reference_v<decltype(f17)> || std::is_reference_v<decltype(f18)> || std::is_reference_v<decltype(f19)> || std::is_reference_v<decltype(f20)> || std::is_reference_v<decltype(f21)> || std::is_reference_v<decltype(f22)> || std::is_reference_v<decltype(f23)> || std::is_reference_v<decltype(f24)> || std::is_reference_v<decltype(f25)> || std::is_reference_v<decltype(f26)> || std::is_reference_v<decltype(f27)> || std::is_reference_v<decltype(f28)> || std::is_reference_v<decltype(f29)> || std::is_reference_v<decltype(f30)> || std::is_reference_v<decltype(f31)> || std::is_reference_v<decltype(f32)> || std::is_reference_v<decltype(f33)> || std::is_reference_v<decltype(f34)> || std::is_reference_v<decltype(f35)> || std::is_reference_v<decltype(f36)> || std::is_reference_v<decltype(f37)> || std::is_reference_v<decltype(f38)>)>{};
       }(std::declval<T&>()));

// Arity 40: decompose into a tuple of references.
template <typename T>
constexpr auto to_tuple_impl(T&& obj, std::integral_constant<std::size_t, 40>) {
    auto&& [f0, f1, f2, f3, f4, f5, f6, f7, f8, f9, f10, f11, f12, f13, f14, f15, f16, f17, f18, f19, f20, f21, f22, f23, f24, f25, f26, f27, f28, f29, f30, f31, f32, f33, f34, f35, f36, f37, f38, f39] = std::forward<T>(obj);
    return std::forward_as_tuple(f0, f1, f2, f3, f4, f5, f6, f7, f8, f9, f10, f11, f12, f13, f14, f15, f16, f17, f18, f19, f20, f21, f22, f23, f24, f25, f26, f27, f28, f29, f30, f31, f32, f33, f34, f35, f36, f37, f38, f39);
}

// Arity 40: reference-member detection for the serializer backstop.
template <typename T>
constexpr auto has_reference_field_tag(T&, std::integral_constant<std::size_t, 40>)
    -> decltype([](T& u) {
           auto& [f0, f1, f2, f3, f4, f5, f6, f7, f8, f9, f10, f11, f12, f13, f14, f15, f16, f17, f18, f19, f20, f21, f22, f23, f24, f25, f26, f27, f28, f29, f30, f31, f32, f33, f34, f35, f36, f37, f38, f39] = u;
           return std::bool_constant<(std::is_reference_v<decltype(f0)> || std::is_reference_v<decltype(f1)> || std::is_reference_v<decltype(f2)> || std::is_reference_v<decltype(f3)> || std::is_reference_v<decltype(f4)> || std::is_reference_v<decltype(f5)> || std::is_reference_v<decltype(f6)> || std::is_reference_v<decltype(f7)> || std::is_reference_v<decltype(f8)> || std::is_reference_v<decltype(f9)> || std::is_reference_v<decltype(f10)> || std::is_reference_v<decltype(f11)> || std::is_reference_v<decltype(f12)> || std::is_reference_v<decltype(f13)> || std::is_reference_v<decltype(f14)> || std::is_reference_v<decltype(f15)> || std::is_reference_v<decltype(f16)> || std::is_reference_v<decltype(f17)> || std::is_reference_v<decltype(f18)> || std::is_reference_v<decltype(f19)> || std::is_reference_v<decltype(f20)> || std::is_reference_v<decltype(f21)> || std::is_reference_v<decltype(f22)> || std::is_reference_v<decltype(f23)> || std::is_reference_v<decltype(f24)> || std::is_reference_v<decltype(f25)> || std::is_reference_v<decltype(f26)> || std::is_reference_v<decltype(f27)> || std::is_reference_v<decltype(f28)> || std::is_reference_v<decltype(f29)> || std::is_reference_v<decltype(f30)> || std::is_reference_v<decltype(f31)> || std::is_reference_v<decltype(f32)> || std::is_reference_v<decltype(f33)> || std::is_reference_v<decltype(f34)> || std::is_reference_v<decltype(f35)> || std::is_reference_v<decltype(f36)> || std::is_reference_v<decltype(f37)> || std::is_reference_v<decltype(f38)> || std::is_reference_v<decltype(f39)>)>{};
       }(std::declval<T&>()));

// Arity 41: decompose into a tuple of references.
template <typename T>
constexpr auto to_tuple_impl(T&& obj, std::integral_constant<std::size_t, 41>) {
    auto&& [f0, f1, f2, f3, f4, f5, f6, f7, f8, f9, f10, f11, f12, f13, f14, f15, f16, f17, f18, f19, f20, f21, f22, f23, f24, f25, f26, f27, f28, f29, f30, f31, f32, f33, f34, f35, f36, f37, f38, f39, f40] = std::forward<T>(obj);
    return std::forward_as_tuple(f0, f1, f2, f3, f4, f5, f6, f7, f8, f9, f10, f11, f12, f13, f14, f15, f16, f17, f18, f19, f20, f21, f22, f23, f24, f25, f26, f27, f28, f29, f30, f31, f32, f33, f34, f35, f36, f37, f38, f39, f40);
}

// Arity 41: reference-member detection for the serializer backstop.
template <typename T>
constexpr auto has_reference_field_tag(T&, std::integral_constant<std::size_t, 41>)
    -> decltype([](T& u) {
           auto& [f0, f1, f2, f3, f4, f5, f6, f7, f8, f9, f10, f11, f12, f13, f14, f15, f16, f17, f18, f19, f20, f21, f22, f23, f24, f25, f26, f27, f28, f29, f30, f31, f32, f33, f34, f35, f36, f37, f38, f39, f40] = u;
           return std::bool_constant<(std::is_reference_v<decltype(f0)> || std::is_reference_v<decltype(f1)> || std::is_reference_v<decltype(f2)> || std::is_reference_v<decltype(f3)> || std::is_reference_v<decltype(f4)> || std::is_reference_v<decltype(f5)> || std::is_reference_v<decltype(f6)> || std::is_reference_v<decltype(f7)> || std::is_reference_v<decltype(f8)> || std::is_reference_v<decltype(f9)> || std::is_reference_v<decltype(f10)> || std::is_reference_v<decltype(f11)> || std::is_reference_v<decltype(f12)> || std::is_reference_v<decltype(f13)> || std::is_reference_v<decltype(f14)> || std::is_reference_v<decltype(f15)> || std::is_reference_v<decltype(f16)> || std::is_reference_v<decltype(f17)> || std::is_reference_v<decltype(f18)> || std::is_reference_v<decltype(f19)> || std::is_reference_v<decltype(f20)> || std::is_reference_v<decltype(f21)> || std::is_reference_v<decltype(f22)> || std::is_reference_v<decltype(f23)> || std::is_reference_v<decltype(f24)> || std::is_reference_v<decltype(f25)> || std::is_reference_v<decltype(f26)> || std::is_reference_v<decltype(f27)> || std::is_reference_v<decltype(f28)> || std::is_reference_v<decltype(f29)> || std::is_reference_v<decltype(f30)> || std::is_reference_v<decltype(f31)> || std::is_reference_v<decltype(f32)> || std::is_reference_v<decltype(f33)> || std::is_reference_v<decltype(f34)> || std::is_reference_v<decltype(f35)> || std::is_reference_v<decltype(f36)> || std::is_reference_v<decltype(f37)> || std::is_reference_v<decltype(f38)> || std::is_reference_v<decltype(f39)> || std::is_reference_v<decltype(f40)>)>{};
       }(std::declval<T&>()));

// Arity 42: decompose into a tuple of references.
template <typename T>
constexpr auto to_tuple_impl(T&& obj, std::integral_constant<std::size_t, 42>) {
    auto&& [f0, f1, f2, f3, f4, f5, f6, f7, f8, f9, f10, f11, f12, f13, f14, f15, f16, f17, f18, f19, f20, f21, f22, f23, f24, f25, f26, f27, f28, f29, f30, f31, f32, f33, f34, f35, f36, f37, f38, f39, f40, f41] = std::forward<T>(obj);
    return std::forward_as_tuple(f0, f1, f2, f3, f4, f5, f6, f7, f8, f9, f10, f11, f12, f13, f14, f15, f16, f17, f18, f19, f20, f21, f22, f23, f24, f25, f26, f27, f28, f29, f30, f31, f32, f33, f34, f35, f36, f37, f38, f39, f40, f41);
}

// Arity 42: reference-member detection for the serializer backstop.
template <typename T>
constexpr auto has_reference_field_tag(T&, std::integral_constant<std::size_t, 42>)
    -> decltype([](T& u) {
           auto& [f0, f1, f2, f3, f4, f5, f6, f7, f8, f9, f10, f11, f12, f13, f14, f15, f16, f17, f18, f19, f20, f21, f22, f23, f24, f25, f26, f27, f28, f29, f30, f31, f32, f33, f34, f35, f36, f37, f38, f39, f40, f41] = u;
           return std::bool_constant<(std::is_reference_v<decltype(f0)> || std::is_reference_v<decltype(f1)> || std::is_reference_v<decltype(f2)> || std::is_reference_v<decltype(f3)> || std::is_reference_v<decltype(f4)> || std::is_reference_v<decltype(f5)> || std::is_reference_v<decltype(f6)> || std::is_reference_v<decltype(f7)> || std::is_reference_v<decltype(f8)> || std::is_reference_v<decltype(f9)> || std::is_reference_v<decltype(f10)> || std::is_reference_v<decltype(f11)> || std::is_reference_v<decltype(f12)> || std::is_reference_v<decltype(f13)> || std::is_reference_v<decltype(f14)> || std::is_reference_v<decltype(f15)> || std::is_reference_v<decltype(f16)> || std::is_reference_v<decltype(f17)> || std::is_reference_v<decltype(f18)> || std::is_reference_v<decltype(f19)> || std::is_reference_v<decltype(f20)> || std::is_reference_v<decltype(f21)> || std::is_reference_v<decltype(f22)> || std::is_reference_v<decltype(f23)> || std::is_reference_v<decltype(f24)> || std::is_reference_v<decltype(f25)> || std::is_reference_v<decltype(f26)> || std::is_reference_v<decltype(f27)> || std::is_reference_v<decltype(f28)> || std::is_reference_v<decltype(f29)> || std::is_reference_v<decltype(f30)> || std::is_reference_v<decltype(f31)> || std::is_reference_v<decltype(f32)> || std::is_reference_v<decltype(f33)> || std::is_reference_v<decltype(f34)> || std::is_reference_v<decltype(f35)> || std::is_reference_v<decltype(f36)> || std::is_reference_v<decltype(f37)> || std::is_reference_v<decltype(f38)> || std::is_reference_v<decltype(f39)> || std::is_reference_v<decltype(f40)> || std::is_reference_v<decltype(f41)>)>{};
       }(std::declval<T&>()));

// Arity 43: decompose into a tuple of references.
template <typename T>
constexpr auto to_tuple_impl(T&& obj, std::integral_constant<std::size_t, 43>) {
    auto&& [f0, f1, f2, f3, f4, f5, f6, f7, f8, f9, f10, f11, f12, f13, f14, f15, f16, f17, f18, f19, f20, f21, f22, f23, f24, f25, f26, f27, f28, f29, f30, f31, f32, f33, f34, f35, f36, f37, f38, f39, f40, f41, f42] = std::forward<T>(obj);
    return std::forward_as_tuple(f0, f1, f2, f3, f4, f5, f6, f7, f8, f9, f10, f11, f12, f13, f14, f15, f16, f17, f18, f19, f20, f21, f22, f23, f24, f25, f26, f27, f28, f29, f30, f31, f32, f33, f34, f35, f36, f37, f38, f39, f40, f41, f42);
}

// Arity 43: reference-member detection for the serializer backstop.
template <typename T>
constexpr auto has_reference_field_tag(T&, std::integral_constant<std::size_t, 43>)
    -> decltype([](T& u) {
           auto& [f0, f1, f2, f3, f4, f5, f6, f7, f8, f9, f10, f11, f12, f13, f14, f15, f16, f17, f18, f19, f20, f21, f22, f23, f24, f25, f26, f27, f28, f29, f30, f31, f32, f33, f34, f35, f36, f37, f38, f39, f40, f41, f42] = u;
           return std::bool_constant<(std::is_reference_v<decltype(f0)> || std::is_reference_v<decltype(f1)> || std::is_reference_v<decltype(f2)> || std::is_reference_v<decltype(f3)> || std::is_reference_v<decltype(f4)> || std::is_reference_v<decltype(f5)> || std::is_reference_v<decltype(f6)> || std::is_reference_v<decltype(f7)> || std::is_reference_v<decltype(f8)> || std::is_reference_v<decltype(f9)> || std::is_reference_v<decltype(f10)> || std::is_reference_v<decltype(f11)> || std::is_reference_v<decltype(f12)> || std::is_reference_v<decltype(f13)> || std::is_reference_v<decltype(f14)> || std::is_reference_v<decltype(f15)> || std::is_reference_v<decltype(f16)> || std::is_reference_v<decltype(f17)> || std::is_reference_v<decltype(f18)> || std::is_reference_v<decltype(f19)> || std::is_reference_v<decltype(f20)> || std::is_reference_v<decltype(f21)> || std::is_reference_v<decltype(f22)> || std::is_reference_v<decltype(f23)> || std::is_reference_v<decltype(f24)> || std::is_reference_v<decltype(f25)> || std::is_reference_v<decltype(f26)> || std::is_reference_v<decltype(f27)> || std::is_reference_v<decltype(f28)> || std::is_reference_v<decltype(f29)> || std::is_reference_v<decltype(f30)> || std::is_reference_v<decltype(f31)> || std::is_reference_v<decltype(f32)> || std::is_reference_v<decltype(f33)> || std::is_reference_v<decltype(f34)> || std::is_reference_v<decltype(f35)> || std::is_reference_v<decltype(f36)> || std::is_reference_v<decltype(f37)> || std::is_reference_v<decltype(f38)> || std::is_reference_v<decltype(f39)> || std::is_reference_v<decltype(f40)> || std::is_reference_v<decltype(f41)> || std::is_reference_v<decltype(f42)>)>{};
       }(std::declval<T&>()));

// Arity 44: decompose into a tuple of references.
template <typename T>
constexpr auto to_tuple_impl(T&& obj, std::integral_constant<std::size_t, 44>) {
    auto&& [f0, f1, f2, f3, f4, f5, f6, f7, f8, f9, f10, f11, f12, f13, f14, f15, f16, f17, f18, f19, f20, f21, f22, f23, f24, f25, f26, f27, f28, f29, f30, f31, f32, f33, f34, f35, f36, f37, f38, f39, f40, f41, f42, f43] = std::forward<T>(obj);
    return std::forward_as_tuple(f0, f1, f2, f3, f4, f5, f6, f7, f8, f9, f10, f11, f12, f13, f14, f15, f16, f17, f18, f19, f20, f21, f22, f23, f24, f25, f26, f27, f28, f29, f30, f31, f32, f33, f34, f35, f36, f37, f38, f39, f40, f41, f42, f43);
}

// Arity 44: reference-member detection for the serializer backstop.
template <typename T>
constexpr auto has_reference_field_tag(T&, std::integral_constant<std::size_t, 44>)
    -> decltype([](T& u) {
           auto& [f0, f1, f2, f3, f4, f5, f6, f7, f8, f9, f10, f11, f12, f13, f14, f15, f16, f17, f18, f19, f20, f21, f22, f23, f24, f25, f26, f27, f28, f29, f30, f31, f32, f33, f34, f35, f36, f37, f38, f39, f40, f41, f42, f43] = u;
           return std::bool_constant<(std::is_reference_v<decltype(f0)> || std::is_reference_v<decltype(f1)> || std::is_reference_v<decltype(f2)> || std::is_reference_v<decltype(f3)> || std::is_reference_v<decltype(f4)> || std::is_reference_v<decltype(f5)> || std::is_reference_v<decltype(f6)> || std::is_reference_v<decltype(f7)> || std::is_reference_v<decltype(f8)> || std::is_reference_v<decltype(f9)> || std::is_reference_v<decltype(f10)> || std::is_reference_v<decltype(f11)> || std::is_reference_v<decltype(f12)> || std::is_reference_v<decltype(f13)> || std::is_reference_v<decltype(f14)> || std::is_reference_v<decltype(f15)> || std::is_reference_v<decltype(f16)> || std::is_reference_v<decltype(f17)> || std::is_reference_v<decltype(f18)> || std::is_reference_v<decltype(f19)> || std::is_reference_v<decltype(f20)> || std::is_reference_v<decltype(f21)> || std::is_reference_v<decltype(f22)> || std::is_reference_v<decltype(f23)> || std::is_reference_v<decltype(f24)> || std::is_reference_v<decltype(f25)> || std::is_reference_v<decltype(f26)> || std::is_reference_v<decltype(f27)> || std::is_reference_v<decltype(f28)> || std::is_reference_v<decltype(f29)> || std::is_reference_v<decltype(f30)> || std::is_reference_v<decltype(f31)> || std::is_reference_v<decltype(f32)> || std::is_reference_v<decltype(f33)> || std::is_reference_v<decltype(f34)> || std::is_reference_v<decltype(f35)> || std::is_reference_v<decltype(f36)> || std::is_reference_v<decltype(f37)> || std::is_reference_v<decltype(f38)> || std::is_reference_v<decltype(f39)> || std::is_reference_v<decltype(f40)> || std::is_reference_v<decltype(f41)> || std::is_reference_v<decltype(f42)> || std::is_reference_v<decltype(f43)>)>{};
       }(std::declval<T&>()));

// Arity 45: decompose into a tuple of references.
template <typename T>
constexpr auto to_tuple_impl(T&& obj, std::integral_constant<std::size_t, 45>) {
    auto&& [f0, f1, f2, f3, f4, f5, f6, f7, f8, f9, f10, f11, f12, f13, f14, f15, f16, f17, f18, f19, f20, f21, f22, f23, f24, f25, f26, f27, f28, f29, f30, f31, f32, f33, f34, f35, f36, f37, f38, f39, f40, f41, f42, f43, f44] = std::forward<T>(obj);
    return std::forward_as_tuple(f0, f1, f2, f3, f4, f5, f6, f7, f8, f9, f10, f11, f12, f13, f14, f15, f16, f17, f18, f19, f20, f21, f22, f23, f24, f25, f26, f27, f28, f29, f30, f31, f32, f33, f34, f35, f36, f37, f38, f39, f40, f41, f42, f43, f44);
}

// Arity 45: reference-member detection for the serializer backstop.
template <typename T>
constexpr auto has_reference_field_tag(T&, std::integral_constant<std::size_t, 45>)
    -> decltype([](T& u) {
           auto& [f0, f1, f2, f3, f4, f5, f6, f7, f8, f9, f10, f11, f12, f13, f14, f15, f16, f17, f18, f19, f20, f21, f22, f23, f24, f25, f26, f27, f28, f29, f30, f31, f32, f33, f34, f35, f36, f37, f38, f39, f40, f41, f42, f43, f44] = u;
           return std::bool_constant<(std::is_reference_v<decltype(f0)> || std::is_reference_v<decltype(f1)> || std::is_reference_v<decltype(f2)> || std::is_reference_v<decltype(f3)> || std::is_reference_v<decltype(f4)> || std::is_reference_v<decltype(f5)> || std::is_reference_v<decltype(f6)> || std::is_reference_v<decltype(f7)> || std::is_reference_v<decltype(f8)> || std::is_reference_v<decltype(f9)> || std::is_reference_v<decltype(f10)> || std::is_reference_v<decltype(f11)> || std::is_reference_v<decltype(f12)> || std::is_reference_v<decltype(f13)> || std::is_reference_v<decltype(f14)> || std::is_reference_v<decltype(f15)> || std::is_reference_v<decltype(f16)> || std::is_reference_v<decltype(f17)> || std::is_reference_v<decltype(f18)> || std::is_reference_v<decltype(f19)> || std::is_reference_v<decltype(f20)> || std::is_reference_v<decltype(f21)> || std::is_reference_v<decltype(f22)> || std::is_reference_v<decltype(f23)> || std::is_reference_v<decltype(f24)> || std::is_reference_v<decltype(f25)> || std::is_reference_v<decltype(f26)> || std::is_reference_v<decltype(f27)> || std::is_reference_v<decltype(f28)> || std::is_reference_v<decltype(f29)> || std::is_reference_v<decltype(f30)> || std::is_reference_v<decltype(f31)> || std::is_reference_v<decltype(f32)> || std::is_reference_v<decltype(f33)> || std::is_reference_v<decltype(f34)> || std::is_reference_v<decltype(f35)> || std::is_reference_v<decltype(f36)> || std::is_reference_v<decltype(f37)> || std::is_reference_v<decltype(f38)> || std::is_reference_v<decltype(f39)> || std::is_reference_v<decltype(f40)> || std::is_reference_v<decltype(f41)> || std::is_reference_v<decltype(f42)> || std::is_reference_v<decltype(f43)> || std::is_reference_v<decltype(f44)>)>{};
       }(std::declval<T&>()));

// Arity 46: decompose into a tuple of references.
template <typename T>
constexpr auto to_tuple_impl(T&& obj, std::integral_constant<std::size_t, 46>) {
    auto&& [f0, f1, f2, f3, f4, f5, f6, f7, f8, f9, f10, f11, f12, f13, f14, f15, f16, f17, f18, f19, f20, f21, f22, f23, f24, f25, f26, f27, f28, f29, f30, f31, f32, f33, f34, f35, f36, f37, f38, f39, f40, f41, f42, f43, f44, f45] = std::forward<T>(obj);
    return std::forward_as_tuple(f0, f1, f2, f3, f4, f5, f6, f7, f8, f9, f10, f11, f12, f13, f14, f15, f16, f17, f18, f19, f20, f21, f22, f23, f24, f25, f26, f27, f28, f29, f30, f31, f32, f33, f34, f35, f36, f37, f38, f39, f40, f41, f42, f43, f44, f45);
}

// Arity 46: reference-member detection for the serializer backstop.
template <typename T>
constexpr auto has_reference_field_tag(T&, std::integral_constant<std::size_t, 46>)
    -> decltype([](T& u) {
           auto& [f0, f1, f2, f3, f4, f5, f6, f7, f8, f9, f10, f11, f12, f13, f14, f15, f16, f17, f18, f19, f20, f21, f22, f23, f24, f25, f26, f27, f28, f29, f30, f31, f32, f33, f34, f35, f36, f37, f38, f39, f40, f41, f42, f43, f44, f45] = u;
           return std::bool_constant<(std::is_reference_v<decltype(f0)> || std::is_reference_v<decltype(f1)> || std::is_reference_v<decltype(f2)> || std::is_reference_v<decltype(f3)> || std::is_reference_v<decltype(f4)> || std::is_reference_v<decltype(f5)> || std::is_reference_v<decltype(f6)> || std::is_reference_v<decltype(f7)> || std::is_reference_v<decltype(f8)> || std::is_reference_v<decltype(f9)> || std::is_reference_v<decltype(f10)> || std::is_reference_v<decltype(f11)> || std::is_reference_v<decltype(f12)> || std::is_reference_v<decltype(f13)> || std::is_reference_v<decltype(f14)> || std::is_reference_v<decltype(f15)> || std::is_reference_v<decltype(f16)> || std::is_reference_v<decltype(f17)> || std::is_reference_v<decltype(f18)> || std::is_reference_v<decltype(f19)> || std::is_reference_v<decltype(f20)> || std::is_reference_v<decltype(f21)> || std::is_reference_v<decltype(f22)> || std::is_reference_v<decltype(f23)> || std::is_reference_v<decltype(f24)> || std::is_reference_v<decltype(f25)> || std::is_reference_v<decltype(f26)> || std::is_reference_v<decltype(f27)> || std::is_reference_v<decltype(f28)> || std::is_reference_v<decltype(f29)> || std::is_reference_v<decltype(f30)> || std::is_reference_v<decltype(f31)> || std::is_reference_v<decltype(f32)> || std::is_reference_v<decltype(f33)> || std::is_reference_v<decltype(f34)> || std::is_reference_v<decltype(f35)> || std::is_reference_v<decltype(f36)> || std::is_reference_v<decltype(f37)> || std::is_reference_v<decltype(f38)> || std::is_reference_v<decltype(f39)> || std::is_reference_v<decltype(f40)> || std::is_reference_v<decltype(f41)> || std::is_reference_v<decltype(f42)> || std::is_reference_v<decltype(f43)> || std::is_reference_v<decltype(f44)> || std::is_reference_v<decltype(f45)>)>{};
       }(std::declval<T&>()));

// Arity 47: decompose into a tuple of references.
template <typename T>
constexpr auto to_tuple_impl(T&& obj, std::integral_constant<std::size_t, 47>) {
    auto&& [f0, f1, f2, f3, f4, f5, f6, f7, f8, f9, f10, f11, f12, f13, f14, f15, f16, f17, f18, f19, f20, f21, f22, f23, f24, f25, f26, f27, f28, f29, f30, f31, f32, f33, f34, f35, f36, f37, f38, f39, f40, f41, f42, f43, f44, f45, f46] = std::forward<T>(obj);
    return std::forward_as_tuple(f0, f1, f2, f3, f4, f5, f6, f7, f8, f9, f10, f11, f12, f13, f14, f15, f16, f17, f18, f19, f20, f21, f22, f23, f24, f25, f26, f27, f28, f29, f30, f31, f32, f33, f34, f35, f36, f37, f38, f39, f40, f41, f42, f43, f44, f45, f46);
}

// Arity 47: reference-member detection for the serializer backstop.
template <typename T>
constexpr auto has_reference_field_tag(T&, std::integral_constant<std::size_t, 47>)
    -> decltype([](T& u) {
           auto& [f0, f1, f2, f3, f4, f5, f6, f7, f8, f9, f10, f11, f12, f13, f14, f15, f16, f17, f18, f19, f20, f21, f22, f23, f24, f25, f26, f27, f28, f29, f30, f31, f32, f33, f34, f35, f36, f37, f38, f39, f40, f41, f42, f43, f44, f45, f46] = u;
           return std::bool_constant<(std::is_reference_v<decltype(f0)> || std::is_reference_v<decltype(f1)> || std::is_reference_v<decltype(f2)> || std::is_reference_v<decltype(f3)> || std::is_reference_v<decltype(f4)> || std::is_reference_v<decltype(f5)> || std::is_reference_v<decltype(f6)> || std::is_reference_v<decltype(f7)> || std::is_reference_v<decltype(f8)> || std::is_reference_v<decltype(f9)> || std::is_reference_v<decltype(f10)> || std::is_reference_v<decltype(f11)> || std::is_reference_v<decltype(f12)> || std::is_reference_v<decltype(f13)> || std::is_reference_v<decltype(f14)> || std::is_reference_v<decltype(f15)> || std::is_reference_v<decltype(f16)> || std::is_reference_v<decltype(f17)> || std::is_reference_v<decltype(f18)> || std::is_reference_v<decltype(f19)> || std::is_reference_v<decltype(f20)> || std::is_reference_v<decltype(f21)> || std::is_reference_v<decltype(f22)> || std::is_reference_v<decltype(f23)> || std::is_reference_v<decltype(f24)> || std::is_reference_v<decltype(f25)> || std::is_reference_v<decltype(f26)> || std::is_reference_v<decltype(f27)> || std::is_reference_v<decltype(f28)> || std::is_reference_v<decltype(f29)> || std::is_reference_v<decltype(f30)> || std::is_reference_v<decltype(f31)> || std::is_reference_v<decltype(f32)> || std::is_reference_v<decltype(f33)> || std::is_reference_v<decltype(f34)> || std::is_reference_v<decltype(f35)> || std::is_reference_v<decltype(f36)> || std::is_reference_v<decltype(f37)> || std::is_reference_v<decltype(f38)> || std::is_reference_v<decltype(f39)> || std::is_reference_v<decltype(f40)> || std::is_reference_v<decltype(f41)> || std::is_reference_v<decltype(f42)> || std::is_reference_v<decltype(f43)> || std::is_reference_v<decltype(f44)> || std::is_reference_v<decltype(f45)> || std::is_reference_v<decltype(f46)>)>{};
       }(std::declval<T&>()));

// Arity 48: decompose into a tuple of references.
template <typename T>
constexpr auto to_tuple_impl(T&& obj, std::integral_constant<std::size_t, 48>) {
    auto&& [f0, f1, f2, f3, f4, f5, f6, f7, f8, f9, f10, f11, f12, f13, f14, f15, f16, f17, f18, f19, f20, f21, f22, f23, f24, f25, f26, f27, f28, f29, f30, f31, f32, f33, f34, f35, f36, f37, f38, f39, f40, f41, f42, f43, f44, f45, f46, f47] = std::forward<T>(obj);
    return std::forward_as_tuple(f0, f1, f2, f3, f4, f5, f6, f7, f8, f9, f10, f11, f12, f13, f14, f15, f16, f17, f18, f19, f20, f21, f22, f23, f24, f25, f26, f27, f28, f29, f30, f31, f32, f33, f34, f35, f36, f37, f38, f39, f40, f41, f42, f43, f44, f45, f46, f47);
}

// Arity 48: reference-member detection for the serializer backstop.
template <typename T>
constexpr auto has_reference_field_tag(T&, std::integral_constant<std::size_t, 48>)
    -> decltype([](T& u) {
           auto& [f0, f1, f2, f3, f4, f5, f6, f7, f8, f9, f10, f11, f12, f13, f14, f15, f16, f17, f18, f19, f20, f21, f22, f23, f24, f25, f26, f27, f28, f29, f30, f31, f32, f33, f34, f35, f36, f37, f38, f39, f40, f41, f42, f43, f44, f45, f46, f47] = u;
           return std::bool_constant<(std::is_reference_v<decltype(f0)> || std::is_reference_v<decltype(f1)> || std::is_reference_v<decltype(f2)> || std::is_reference_v<decltype(f3)> || std::is_reference_v<decltype(f4)> || std::is_reference_v<decltype(f5)> || std::is_reference_v<decltype(f6)> || std::is_reference_v<decltype(f7)> || std::is_reference_v<decltype(f8)> || std::is_reference_v<decltype(f9)> || std::is_reference_v<decltype(f10)> || std::is_reference_v<decltype(f11)> || std::is_reference_v<decltype(f12)> || std::is_reference_v<decltype(f13)> || std::is_reference_v<decltype(f14)> || std::is_reference_v<decltype(f15)> || std::is_reference_v<decltype(f16)> || std::is_reference_v<decltype(f17)> || std::is_reference_v<decltype(f18)> || std::is_reference_v<decltype(f19)> || std::is_reference_v<decltype(f20)> || std::is_reference_v<decltype(f21)> || std::is_reference_v<decltype(f22)> || std::is_reference_v<decltype(f23)> || std::is_reference_v<decltype(f24)> || std::is_reference_v<decltype(f25)> || std::is_reference_v<decltype(f26)> || std::is_reference_v<decltype(f27)> || std::is_reference_v<decltype(f28)> || std::is_reference_v<decltype(f29)> || std::is_reference_v<decltype(f30)> || std::is_reference_v<decltype(f31)> || std::is_reference_v<decltype(f32)> || std::is_reference_v<decltype(f33)> || std::is_reference_v<decltype(f34)> || std::is_reference_v<decltype(f35)> || std::is_reference_v<decltype(f36)> || std::is_reference_v<decltype(f37)> || std::is_reference_v<decltype(f38)> || std::is_reference_v<decltype(f39)> || std::is_reference_v<decltype(f40)> || std::is_reference_v<decltype(f41)> || std::is_reference_v<decltype(f42)> || std::is_reference_v<decltype(f43)> || std::is_reference_v<decltype(f44)> || std::is_reference_v<decltype(f45)> || std::is_reference_v<decltype(f46)> || std::is_reference_v<decltype(f47)>)>{};
       }(std::declval<T&>()));

// Arity 49: decompose into a tuple of references.
template <typename T>
constexpr auto to_tuple_impl(T&& obj, std::integral_constant<std::size_t, 49>) {
    auto&& [f0, f1, f2, f3, f4, f5, f6, f7, f8, f9, f10, f11, f12, f13, f14, f15, f16, f17, f18, f19, f20, f21, f22, f23, f24, f25, f26, f27, f28, f29, f30, f31, f32, f33, f34, f35, f36, f37, f38, f39, f40, f41, f42, f43, f44, f45, f46, f47, f48] = std::forward<T>(obj);
    return std::forward_as_tuple(f0, f1, f2, f3, f4, f5, f6, f7, f8, f9, f10, f11, f12, f13, f14, f15, f16, f17, f18, f19, f20, f21, f22, f23, f24, f25, f26, f27, f28, f29, f30, f31, f32, f33, f34, f35, f36, f37, f38, f39, f40, f41, f42, f43, f44, f45, f46, f47, f48);
}

// Arity 49: reference-member detection for the serializer backstop.
template <typename T>
constexpr auto has_reference_field_tag(T&, std::integral_constant<std::size_t, 49>)
    -> decltype([](T& u) {
           auto& [f0, f1, f2, f3, f4, f5, f6, f7, f8, f9, f10, f11, f12, f13, f14, f15, f16, f17, f18, f19, f20, f21, f22, f23, f24, f25, f26, f27, f28, f29, f30, f31, f32, f33, f34, f35, f36, f37, f38, f39, f40, f41, f42, f43, f44, f45, f46, f47, f48] = u;
           return std::bool_constant<(std::is_reference_v<decltype(f0)> || std::is_reference_v<decltype(f1)> || std::is_reference_v<decltype(f2)> || std::is_reference_v<decltype(f3)> || std::is_reference_v<decltype(f4)> || std::is_reference_v<decltype(f5)> || std::is_reference_v<decltype(f6)> || std::is_reference_v<decltype(f7)> || std::is_reference_v<decltype(f8)> || std::is_reference_v<decltype(f9)> || std::is_reference_v<decltype(f10)> || std::is_reference_v<decltype(f11)> || std::is_reference_v<decltype(f12)> || std::is_reference_v<decltype(f13)> || std::is_reference_v<decltype(f14)> || std::is_reference_v<decltype(f15)> || std::is_reference_v<decltype(f16)> || std::is_reference_v<decltype(f17)> || std::is_reference_v<decltype(f18)> || std::is_reference_v<decltype(f19)> || std::is_reference_v<decltype(f20)> || std::is_reference_v<decltype(f21)> || std::is_reference_v<decltype(f22)> || std::is_reference_v<decltype(f23)> || std::is_reference_v<decltype(f24)> || std::is_reference_v<decltype(f25)> || std::is_reference_v<decltype(f26)> || std::is_reference_v<decltype(f27)> || std::is_reference_v<decltype(f28)> || std::is_reference_v<decltype(f29)> || std::is_reference_v<decltype(f30)> || std::is_reference_v<decltype(f31)> || std::is_reference_v<decltype(f32)> || std::is_reference_v<decltype(f33)> || std::is_reference_v<decltype(f34)> || std::is_reference_v<decltype(f35)> || std::is_reference_v<decltype(f36)> || std::is_reference_v<decltype(f37)> || std::is_reference_v<decltype(f38)> || std::is_reference_v<decltype(f39)> || std::is_reference_v<decltype(f40)> || std::is_reference_v<decltype(f41)> || std::is_reference_v<decltype(f42)> || std::is_reference_v<decltype(f43)> || std::is_reference_v<decltype(f44)> || std::is_reference_v<decltype(f45)> || std::is_reference_v<decltype(f46)> || std::is_reference_v<decltype(f47)> || std::is_reference_v<decltype(f48)>)>{};
       }(std::declval<T&>()));

// Arity 50: decompose into a tuple of references.
template <typename T>
constexpr auto to_tuple_impl(T&& obj, std::integral_constant<std::size_t, 50>) {
    auto&& [f0, f1, f2, f3, f4, f5, f6, f7, f8, f9, f10, f11, f12, f13, f14, f15, f16, f17, f18, f19, f20, f21, f22, f23, f24, f25, f26, f27, f28, f29, f30, f31, f32, f33, f34, f35, f36, f37, f38, f39, f40, f41, f42, f43, f44, f45, f46, f47, f48, f49] = std::forward<T>(obj);
    return std::forward_as_tuple(f0, f1, f2, f3, f4, f5, f6, f7, f8, f9, f10, f11, f12, f13, f14, f15, f16, f17, f18, f19, f20, f21, f22, f23, f24, f25, f26, f27, f28, f29, f30, f31, f32, f33, f34, f35, f36, f37, f38, f39, f40, f41, f42, f43, f44, f45, f46, f47, f48, f49);
}

// Arity 50: reference-member detection for the serializer backstop.
template <typename T>
constexpr auto has_reference_field_tag(T&, std::integral_constant<std::size_t, 50>)
    -> decltype([](T& u) {
           auto& [f0, f1, f2, f3, f4, f5, f6, f7, f8, f9, f10, f11, f12, f13, f14, f15, f16, f17, f18, f19, f20, f21, f22, f23, f24, f25, f26, f27, f28, f29, f30, f31, f32, f33, f34, f35, f36, f37, f38, f39, f40, f41, f42, f43, f44, f45, f46, f47, f48, f49] = u;
           return std::bool_constant<(std::is_reference_v<decltype(f0)> || std::is_reference_v<decltype(f1)> || std::is_reference_v<decltype(f2)> || std::is_reference_v<decltype(f3)> || std::is_reference_v<decltype(f4)> || std::is_reference_v<decltype(f5)> || std::is_reference_v<decltype(f6)> || std::is_reference_v<decltype(f7)> || std::is_reference_v<decltype(f8)> || std::is_reference_v<decltype(f9)> || std::is_reference_v<decltype(f10)> || std::is_reference_v<decltype(f11)> || std::is_reference_v<decltype(f12)> || std::is_reference_v<decltype(f13)> || std::is_reference_v<decltype(f14)> || std::is_reference_v<decltype(f15)> || std::is_reference_v<decltype(f16)> || std::is_reference_v<decltype(f17)> || std::is_reference_v<decltype(f18)> || std::is_reference_v<decltype(f19)> || std::is_reference_v<decltype(f20)> || std::is_reference_v<decltype(f21)> || std::is_reference_v<decltype(f22)> || std::is_reference_v<decltype(f23)> || std::is_reference_v<decltype(f24)> || std::is_reference_v<decltype(f25)> || std::is_reference_v<decltype(f26)> || std::is_reference_v<decltype(f27)> || std::is_reference_v<decltype(f28)> || std::is_reference_v<decltype(f29)> || std::is_reference_v<decltype(f30)> || std::is_reference_v<decltype(f31)> || std::is_reference_v<decltype(f32)> || std::is_reference_v<decltype(f33)> || std::is_reference_v<decltype(f34)> || std::is_reference_v<decltype(f35)> || std::is_reference_v<decltype(f36)> || std::is_reference_v<decltype(f37)> || std::is_reference_v<decltype(f38)> || std::is_reference_v<decltype(f39)> || std::is_reference_v<decltype(f40)> || std::is_reference_v<decltype(f41)> || std::is_reference_v<decltype(f42)> || std::is_reference_v<decltype(f43)> || std::is_reference_v<decltype(f44)> || std::is_reference_v<decltype(f45)> || std::is_reference_v<decltype(f46)> || std::is_reference_v<decltype(f47)> || std::is_reference_v<decltype(f48)> || std::is_reference_v<decltype(f49)>)>{};
       }(std::declval<T&>()));

// Arity 51: decompose into a tuple of references.
template <typename T>
constexpr auto to_tuple_impl(T&& obj, std::integral_constant<std::size_t, 51>) {
    auto&& [f0, f1, f2, f3, f4, f5, f6, f7, f8, f9, f10, f11, f12, f13, f14, f15, f16, f17, f18, f19, f20, f21, f22, f23, f24, f25, f26, f27, f28, f29, f30, f31, f32, f33, f34, f35, f36, f37, f38, f39, f40, f41, f42, f43, f44, f45, f46, f47, f48, f49, f50] = std::forward<T>(obj);
    return std::forward_as_tuple(f0, f1, f2, f3, f4, f5, f6, f7, f8, f9, f10, f11, f12, f13, f14, f15, f16, f17, f18, f19, f20, f21, f22, f23, f24, f25, f26, f27, f28, f29, f30, f31, f32, f33, f34, f35, f36, f37, f38, f39, f40, f41, f42, f43, f44, f45, f46, f47, f48, f49, f50);
}

// Arity 51: reference-member detection for the serializer backstop.
template <typename T>
constexpr auto has_reference_field_tag(T&, std::integral_constant<std::size_t, 51>)
    -> decltype([](T& u) {
           auto& [f0, f1, f2, f3, f4, f5, f6, f7, f8, f9, f10, f11, f12, f13, f14, f15, f16, f17, f18, f19, f20, f21, f22, f23, f24, f25, f26, f27, f28, f29, f30, f31, f32, f33, f34, f35, f36, f37, f38, f39, f40, f41, f42, f43, f44, f45, f46, f47, f48, f49, f50] = u;
           return std::bool_constant<(std::is_reference_v<decltype(f0)> || std::is_reference_v<decltype(f1)> || std::is_reference_v<decltype(f2)> || std::is_reference_v<decltype(f3)> || std::is_reference_v<decltype(f4)> || std::is_reference_v<decltype(f5)> || std::is_reference_v<decltype(f6)> || std::is_reference_v<decltype(f7)> || std::is_reference_v<decltype(f8)> || std::is_reference_v<decltype(f9)> || std::is_reference_v<decltype(f10)> || std::is_reference_v<decltype(f11)> || std::is_reference_v<decltype(f12)> || std::is_reference_v<decltype(f13)> || std::is_reference_v<decltype(f14)> || std::is_reference_v<decltype(f15)> || std::is_reference_v<decltype(f16)> || std::is_reference_v<decltype(f17)> || std::is_reference_v<decltype(f18)> || std::is_reference_v<decltype(f19)> || std::is_reference_v<decltype(f20)> || std::is_reference_v<decltype(f21)> || std::is_reference_v<decltype(f22)> || std::is_reference_v<decltype(f23)> || std::is_reference_v<decltype(f24)> || std::is_reference_v<decltype(f25)> || std::is_reference_v<decltype(f26)> || std::is_reference_v<decltype(f27)> || std::is_reference_v<decltype(f28)> || std::is_reference_v<decltype(f29)> || std::is_reference_v<decltype(f30)> || std::is_reference_v<decltype(f31)> || std::is_reference_v<decltype(f32)> || std::is_reference_v<decltype(f33)> || std::is_reference_v<decltype(f34)> || std::is_reference_v<decltype(f35)> || std::is_reference_v<decltype(f36)> || std::is_reference_v<decltype(f37)> || std::is_reference_v<decltype(f38)> || std::is_reference_v<decltype(f39)> || std::is_reference_v<decltype(f40)> || std::is_reference_v<decltype(f41)> || std::is_reference_v<decltype(f42)> || std::is_reference_v<decltype(f43)> || std::is_reference_v<decltype(f44)> || std::is_reference_v<decltype(f45)> || std::is_reference_v<decltype(f46)> || std::is_reference_v<decltype(f47)> || std::is_reference_v<decltype(f48)> || std::is_reference_v<decltype(f49)> || std::is_reference_v<decltype(f50)>)>{};
       }(std::declval<T&>()));

// Arity 52: decompose into a tuple of references.
template <typename T>
constexpr auto to_tuple_impl(T&& obj, std::integral_constant<std::size_t, 52>) {
    auto&& [f0, f1, f2, f3, f4, f5, f6, f7, f8, f9, f10, f11, f12, f13, f14, f15, f16, f17, f18, f19, f20, f21, f22, f23, f24, f25, f26, f27, f28, f29, f30, f31, f32, f33, f34, f35, f36, f37, f38, f39, f40, f41, f42, f43, f44, f45, f46, f47, f48, f49, f50, f51] = std::forward<T>(obj);
    return std::forward_as_tuple(f0, f1, f2, f3, f4, f5, f6, f7, f8, f9, f10, f11, f12, f13, f14, f15, f16, f17, f18, f19, f20, f21, f22, f23, f24, f25, f26, f27, f28, f29, f30, f31, f32, f33, f34, f35, f36, f37, f38, f39, f40, f41, f42, f43, f44, f45, f46, f47, f48, f49, f50, f51);
}

// Arity 52: reference-member detection for the serializer backstop.
template <typename T>
constexpr auto has_reference_field_tag(T&, std::integral_constant<std::size_t, 52>)
    -> decltype([](T& u) {
           auto& [f0, f1, f2, f3, f4, f5, f6, f7, f8, f9, f10, f11, f12, f13, f14, f15, f16, f17, f18, f19, f20, f21, f22, f23, f24, f25, f26, f27, f28, f29, f30, f31, f32, f33, f34, f35, f36, f37, f38, f39, f40, f41, f42, f43, f44, f45, f46, f47, f48, f49, f50, f51] = u;
           return std::bool_constant<(std::is_reference_v<decltype(f0)> || std::is_reference_v<decltype(f1)> || std::is_reference_v<decltype(f2)> || std::is_reference_v<decltype(f3)> || std::is_reference_v<decltype(f4)> || std::is_reference_v<decltype(f5)> || std::is_reference_v<decltype(f6)> || std::is_reference_v<decltype(f7)> || std::is_reference_v<decltype(f8)> || std::is_reference_v<decltype(f9)> || std::is_reference_v<decltype(f10)> || std::is_reference_v<decltype(f11)> || std::is_reference_v<decltype(f12)> || std::is_reference_v<decltype(f13)> || std::is_reference_v<decltype(f14)> || std::is_reference_v<decltype(f15)> || std::is_reference_v<decltype(f16)> || std::is_reference_v<decltype(f17)> || std::is_reference_v<decltype(f18)> || std::is_reference_v<decltype(f19)> || std::is_reference_v<decltype(f20)> || std::is_reference_v<decltype(f21)> || std::is_reference_v<decltype(f22)> || std::is_reference_v<decltype(f23)> || std::is_reference_v<decltype(f24)> || std::is_reference_v<decltype(f25)> || std::is_reference_v<decltype(f26)> || std::is_reference_v<decltype(f27)> || std::is_reference_v<decltype(f28)> || std::is_reference_v<decltype(f29)> || std::is_reference_v<decltype(f30)> || std::is_reference_v<decltype(f31)> || std::is_reference_v<decltype(f32)> || std::is_reference_v<decltype(f33)> || std::is_reference_v<decltype(f34)> || std::is_reference_v<decltype(f35)> || std::is_reference_v<decltype(f36)> || std::is_reference_v<decltype(f37)> || std::is_reference_v<decltype(f38)> || std::is_reference_v<decltype(f39)> || std::is_reference_v<decltype(f40)> || std::is_reference_v<decltype(f41)> || std::is_reference_v<decltype(f42)> || std::is_reference_v<decltype(f43)> || std::is_reference_v<decltype(f44)> || std::is_reference_v<decltype(f45)> || std::is_reference_v<decltype(f46)> || std::is_reference_v<decltype(f47)> || std::is_reference_v<decltype(f48)> || std::is_reference_v<decltype(f49)> || std::is_reference_v<decltype(f50)> || std::is_reference_v<decltype(f51)>)>{};
       }(std::declval<T&>()));

// Arity 53: decompose into a tuple of references.
template <typename T>
constexpr auto to_tuple_impl(T&& obj, std::integral_constant<std::size_t, 53>) {
    auto&& [f0, f1, f2, f3, f4, f5, f6, f7, f8, f9, f10, f11, f12, f13, f14, f15, f16, f17, f18, f19, f20, f21, f22, f23, f24, f25, f26, f27, f28, f29, f30, f31, f32, f33, f34, f35, f36, f37, f38, f39, f40, f41, f42, f43, f44, f45, f46, f47, f48, f49, f50, f51, f52] = std::forward<T>(obj);
    return std::forward_as_tuple(f0, f1, f2, f3, f4, f5, f6, f7, f8, f9, f10, f11, f12, f13, f14, f15, f16, f17, f18, f19, f20, f21, f22, f23, f24, f25, f26, f27, f28, f29, f30, f31, f32, f33, f34, f35, f36, f37, f38, f39, f40, f41, f42, f43, f44, f45, f46, f47, f48, f49, f50, f51, f52);
}

// Arity 53: reference-member detection for the serializer backstop.
template <typename T>
constexpr auto has_reference_field_tag(T&, std::integral_constant<std::size_t, 53>)
    -> decltype([](T& u) {
           auto& [f0, f1, f2, f3, f4, f5, f6, f7, f8, f9, f10, f11, f12, f13, f14, f15, f16, f17, f18, f19, f20, f21, f22, f23, f24, f25, f26, f27, f28, f29, f30, f31, f32, f33, f34, f35, f36, f37, f38, f39, f40, f41, f42, f43, f44, f45, f46, f47, f48, f49, f50, f51, f52] = u;
           return std::bool_constant<(std::is_reference_v<decltype(f0)> || std::is_reference_v<decltype(f1)> || std::is_reference_v<decltype(f2)> || std::is_reference_v<decltype(f3)> || std::is_reference_v<decltype(f4)> || std::is_reference_v<decltype(f5)> || std::is_reference_v<decltype(f6)> || std::is_reference_v<decltype(f7)> || std::is_reference_v<decltype(f8)> || std::is_reference_v<decltype(f9)> || std::is_reference_v<decltype(f10)> || std::is_reference_v<decltype(f11)> || std::is_reference_v<decltype(f12)> || std::is_reference_v<decltype(f13)> || std::is_reference_v<decltype(f14)> || std::is_reference_v<decltype(f15)> || std::is_reference_v<decltype(f16)> || std::is_reference_v<decltype(f17)> || std::is_reference_v<decltype(f18)> || std::is_reference_v<decltype(f19)> || std::is_reference_v<decltype(f20)> || std::is_reference_v<decltype(f21)> || std::is_reference_v<decltype(f22)> || std::is_reference_v<decltype(f23)> || std::is_reference_v<decltype(f24)> || std::is_reference_v<decltype(f25)> || std::is_reference_v<decltype(f26)> || std::is_reference_v<decltype(f27)> || std::is_reference_v<decltype(f28)> || std::is_reference_v<decltype(f29)> || std::is_reference_v<decltype(f30)> || std::is_reference_v<decltype(f31)> || std::is_reference_v<decltype(f32)> || std::is_reference_v<decltype(f33)> || std::is_reference_v<decltype(f34)> || std::is_reference_v<decltype(f35)> || std::is_reference_v<decltype(f36)> || std::is_reference_v<decltype(f37)> || std::is_reference_v<decltype(f38)> || std::is_reference_v<decltype(f39)> || std::is_reference_v<decltype(f40)> || std::is_reference_v<decltype(f41)> || std::is_reference_v<decltype(f42)> || std::is_reference_v<decltype(f43)> || std::is_reference_v<decltype(f44)> || std::is_reference_v<decltype(f45)> || std::is_reference_v<decltype(f46)> || std::is_reference_v<decltype(f47)> || std::is_reference_v<decltype(f48)> || std::is_reference_v<decltype(f49)> || std::is_reference_v<decltype(f50)> || std::is_reference_v<decltype(f51)> || std::is_reference_v<decltype(f52)>)>{};
       }(std::declval<T&>()));

// Arity 54: decompose into a tuple of references.
template <typename T>
constexpr auto to_tuple_impl(T&& obj, std::integral_constant<std::size_t, 54>) {
    auto&& [f0, f1, f2, f3, f4, f5, f6, f7, f8, f9, f10, f11, f12, f13, f14, f15, f16, f17, f18, f19, f20, f21, f22, f23, f24, f25, f26, f27, f28, f29, f30, f31, f32, f33, f34, f35, f36, f37, f38, f39, f40, f41, f42, f43, f44, f45, f46, f47, f48, f49, f50, f51, f52, f53] = std::forward<T>(obj);
    return std::forward_as_tuple(f0, f1, f2, f3, f4, f5, f6, f7, f8, f9, f10, f11, f12, f13, f14, f15, f16, f17, f18, f19, f20, f21, f22, f23, f24, f25, f26, f27, f28, f29, f30, f31, f32, f33, f34, f35, f36, f37, f38, f39, f40, f41, f42, f43, f44, f45, f46, f47, f48, f49, f50, f51, f52, f53);
}

// Arity 54: reference-member detection for the serializer backstop.
template <typename T>
constexpr auto has_reference_field_tag(T&, std::integral_constant<std::size_t, 54>)
    -> decltype([](T& u) {
           auto& [f0, f1, f2, f3, f4, f5, f6, f7, f8, f9, f10, f11, f12, f13, f14, f15, f16, f17, f18, f19, f20, f21, f22, f23, f24, f25, f26, f27, f28, f29, f30, f31, f32, f33, f34, f35, f36, f37, f38, f39, f40, f41, f42, f43, f44, f45, f46, f47, f48, f49, f50, f51, f52, f53] = u;
           return std::bool_constant<(std::is_reference_v<decltype(f0)> || std::is_reference_v<decltype(f1)> || std::is_reference_v<decltype(f2)> || std::is_reference_v<decltype(f3)> || std::is_reference_v<decltype(f4)> || std::is_reference_v<decltype(f5)> || std::is_reference_v<decltype(f6)> || std::is_reference_v<decltype(f7)> || std::is_reference_v<decltype(f8)> || std::is_reference_v<decltype(f9)> || std::is_reference_v<decltype(f10)> || std::is_reference_v<decltype(f11)> || std::is_reference_v<decltype(f12)> || std::is_reference_v<decltype(f13)> || std::is_reference_v<decltype(f14)> || std::is_reference_v<decltype(f15)> || std::is_reference_v<decltype(f16)> || std::is_reference_v<decltype(f17)> || std::is_reference_v<decltype(f18)> || std::is_reference_v<decltype(f19)> || std::is_reference_v<decltype(f20)> || std::is_reference_v<decltype(f21)> || std::is_reference_v<decltype(f22)> || std::is_reference_v<decltype(f23)> || std::is_reference_v<decltype(f24)> || std::is_reference_v<decltype(f25)> || std::is_reference_v<decltype(f26)> || std::is_reference_v<decltype(f27)> || std::is_reference_v<decltype(f28)> || std::is_reference_v<decltype(f29)> || std::is_reference_v<decltype(f30)> || std::is_reference_v<decltype(f31)> || std::is_reference_v<decltype(f32)> || std::is_reference_v<decltype(f33)> || std::is_reference_v<decltype(f34)> || std::is_reference_v<decltype(f35)> || std::is_reference_v<decltype(f36)> || std::is_reference_v<decltype(f37)> || std::is_reference_v<decltype(f38)> || std::is_reference_v<decltype(f39)> || std::is_reference_v<decltype(f40)> || std::is_reference_v<decltype(f41)> || std::is_reference_v<decltype(f42)> || std::is_reference_v<decltype(f43)> || std::is_reference_v<decltype(f44)> || std::is_reference_v<decltype(f45)> || std::is_reference_v<decltype(f46)> || std::is_reference_v<decltype(f47)> || std::is_reference_v<decltype(f48)> || std::is_reference_v<decltype(f49)> || std::is_reference_v<decltype(f50)> || std::is_reference_v<decltype(f51)> || std::is_reference_v<decltype(f52)> || std::is_reference_v<decltype(f53)>)>{};
       }(std::declval<T&>()));

// Arity 55: decompose into a tuple of references.
template <typename T>
constexpr auto to_tuple_impl(T&& obj, std::integral_constant<std::size_t, 55>) {
    auto&& [f0, f1, f2, f3, f4, f5, f6, f7, f8, f9, f10, f11, f12, f13, f14, f15, f16, f17, f18, f19, f20, f21, f22, f23, f24, f25, f26, f27, f28, f29, f30, f31, f32, f33, f34, f35, f36, f37, f38, f39, f40, f41, f42, f43, f44, f45, f46, f47, f48, f49, f50, f51, f52, f53, f54] = std::forward<T>(obj);
    return std::forward_as_tuple(f0, f1, f2, f3, f4, f5, f6, f7, f8, f9, f10, f11, f12, f13, f14, f15, f16, f17, f18, f19, f20, f21, f22, f23, f24, f25, f26, f27, f28, f29, f30, f31, f32, f33, f34, f35, f36, f37, f38, f39, f40, f41, f42, f43, f44, f45, f46, f47, f48, f49, f50, f51, f52, f53, f54);
}

// Arity 55: reference-member detection for the serializer backstop.
template <typename T>
constexpr auto has_reference_field_tag(T&, std::integral_constant<std::size_t, 55>)
    -> decltype([](T& u) {
           auto& [f0, f1, f2, f3, f4, f5, f6, f7, f8, f9, f10, f11, f12, f13, f14, f15, f16, f17, f18, f19, f20, f21, f22, f23, f24, f25, f26, f27, f28, f29, f30, f31, f32, f33, f34, f35, f36, f37, f38, f39, f40, f41, f42, f43, f44, f45, f46, f47, f48, f49, f50, f51, f52, f53, f54] = u;
           return std::bool_constant<(std::is_reference_v<decltype(f0)> || std::is_reference_v<decltype(f1)> || std::is_reference_v<decltype(f2)> || std::is_reference_v<decltype(f3)> || std::is_reference_v<decltype(f4)> || std::is_reference_v<decltype(f5)> || std::is_reference_v<decltype(f6)> || std::is_reference_v<decltype(f7)> || std::is_reference_v<decltype(f8)> || std::is_reference_v<decltype(f9)> || std::is_reference_v<decltype(f10)> || std::is_reference_v<decltype(f11)> || std::is_reference_v<decltype(f12)> || std::is_reference_v<decltype(f13)> || std::is_reference_v<decltype(f14)> || std::is_reference_v<decltype(f15)> || std::is_reference_v<decltype(f16)> || std::is_reference_v<decltype(f17)> || std::is_reference_v<decltype(f18)> || std::is_reference_v<decltype(f19)> || std::is_reference_v<decltype(f20)> || std::is_reference_v<decltype(f21)> || std::is_reference_v<decltype(f22)> || std::is_reference_v<decltype(f23)> || std::is_reference_v<decltype(f24)> || std::is_reference_v<decltype(f25)> || std::is_reference_v<decltype(f26)> || std::is_reference_v<decltype(f27)> || std::is_reference_v<decltype(f28)> || std::is_reference_v<decltype(f29)> || std::is_reference_v<decltype(f30)> || std::is_reference_v<decltype(f31)> || std::is_reference_v<decltype(f32)> || std::is_reference_v<decltype(f33)> || std::is_reference_v<decltype(f34)> || std::is_reference_v<decltype(f35)> || std::is_reference_v<decltype(f36)> || std::is_reference_v<decltype(f37)> || std::is_reference_v<decltype(f38)> || std::is_reference_v<decltype(f39)> || std::is_reference_v<decltype(f40)> || std::is_reference_v<decltype(f41)> || std::is_reference_v<decltype(f42)> || std::is_reference_v<decltype(f43)> || std::is_reference_v<decltype(f44)> || std::is_reference_v<decltype(f45)> || std::is_reference_v<decltype(f46)> || std::is_reference_v<decltype(f47)> || std::is_reference_v<decltype(f48)> || std::is_reference_v<decltype(f49)> || std::is_reference_v<decltype(f50)> || std::is_reference_v<decltype(f51)> || std::is_reference_v<decltype(f52)> || std::is_reference_v<decltype(f53)> || std::is_reference_v<decltype(f54)>)>{};
       }(std::declval<T&>()));

// Arity 56: decompose into a tuple of references.
template <typename T>
constexpr auto to_tuple_impl(T&& obj, std::integral_constant<std::size_t, 56>) {
    auto&& [f0, f1, f2, f3, f4, f5, f6, f7, f8, f9, f10, f11, f12, f13, f14, f15, f16, f17, f18, f19, f20, f21, f22, f23, f24, f25, f26, f27, f28, f29, f30, f31, f32, f33, f34, f35, f36, f37, f38, f39, f40, f41, f42, f43, f44, f45, f46, f47, f48, f49, f50, f51, f52, f53, f54, f55] = std::forward<T>(obj);
    return std::forward_as_tuple(f0, f1, f2, f3, f4, f5, f6, f7, f8, f9, f10, f11, f12, f13, f14, f15, f16, f17, f18, f19, f20, f21, f22, f23, f24, f25, f26, f27, f28, f29, f30, f31, f32, f33, f34, f35, f36, f37, f38, f39, f40, f41, f42, f43, f44, f45, f46, f47, f48, f49, f50, f51, f52, f53, f54, f55);
}

// Arity 56: reference-member detection for the serializer backstop.
template <typename T>
constexpr auto has_reference_field_tag(T&, std::integral_constant<std::size_t, 56>)
    -> decltype([](T& u) {
           auto& [f0, f1, f2, f3, f4, f5, f6, f7, f8, f9, f10, f11, f12, f13, f14, f15, f16, f17, f18, f19, f20, f21, f22, f23, f24, f25, f26, f27, f28, f29, f30, f31, f32, f33, f34, f35, f36, f37, f38, f39, f40, f41, f42, f43, f44, f45, f46, f47, f48, f49, f50, f51, f52, f53, f54, f55] = u;
           return std::bool_constant<(std::is_reference_v<decltype(f0)> || std::is_reference_v<decltype(f1)> || std::is_reference_v<decltype(f2)> || std::is_reference_v<decltype(f3)> || std::is_reference_v<decltype(f4)> || std::is_reference_v<decltype(f5)> || std::is_reference_v<decltype(f6)> || std::is_reference_v<decltype(f7)> || std::is_reference_v<decltype(f8)> || std::is_reference_v<decltype(f9)> || std::is_reference_v<decltype(f10)> || std::is_reference_v<decltype(f11)> || std::is_reference_v<decltype(f12)> || std::is_reference_v<decltype(f13)> || std::is_reference_v<decltype(f14)> || std::is_reference_v<decltype(f15)> || std::is_reference_v<decltype(f16)> || std::is_reference_v<decltype(f17)> || std::is_reference_v<decltype(f18)> || std::is_reference_v<decltype(f19)> || std::is_reference_v<decltype(f20)> || std::is_reference_v<decltype(f21)> || std::is_reference_v<decltype(f22)> || std::is_reference_v<decltype(f23)> || std::is_reference_v<decltype(f24)> || std::is_reference_v<decltype(f25)> || std::is_reference_v<decltype(f26)> || std::is_reference_v<decltype(f27)> || std::is_reference_v<decltype(f28)> || std::is_reference_v<decltype(f29)> || std::is_reference_v<decltype(f30)> || std::is_reference_v<decltype(f31)> || std::is_reference_v<decltype(f32)> || std::is_reference_v<decltype(f33)> || std::is_reference_v<decltype(f34)> || std::is_reference_v<decltype(f35)> || std::is_reference_v<decltype(f36)> || std::is_reference_v<decltype(f37)> || std::is_reference_v<decltype(f38)> || std::is_reference_v<decltype(f39)> || std::is_reference_v<decltype(f40)> || std::is_reference_v<decltype(f41)> || std::is_reference_v<decltype(f42)> || std::is_reference_v<decltype(f43)> || std::is_reference_v<decltype(f44)> || std::is_reference_v<decltype(f45)> || std::is_reference_v<decltype(f46)> || std::is_reference_v<decltype(f47)> || std::is_reference_v<decltype(f48)> || std::is_reference_v<decltype(f49)> || std::is_reference_v<decltype(f50)> || std::is_reference_v<decltype(f51)> || std::is_reference_v<decltype(f52)> || std::is_reference_v<decltype(f53)> || std::is_reference_v<decltype(f54)> || std::is_reference_v<decltype(f55)>)>{};
       }(std::declval<T&>()));

// Arity 57: decompose into a tuple of references.
template <typename T>
constexpr auto to_tuple_impl(T&& obj, std::integral_constant<std::size_t, 57>) {
    auto&& [f0, f1, f2, f3, f4, f5, f6, f7, f8, f9, f10, f11, f12, f13, f14, f15, f16, f17, f18, f19, f20, f21, f22, f23, f24, f25, f26, f27, f28, f29, f30, f31, f32, f33, f34, f35, f36, f37, f38, f39, f40, f41, f42, f43, f44, f45, f46, f47, f48, f49, f50, f51, f52, f53, f54, f55, f56] = std::forward<T>(obj);
    return std::forward_as_tuple(f0, f1, f2, f3, f4, f5, f6, f7, f8, f9, f10, f11, f12, f13, f14, f15, f16, f17, f18, f19, f20, f21, f22, f23, f24, f25, f26, f27, f28, f29, f30, f31, f32, f33, f34, f35, f36, f37, f38, f39, f40, f41, f42, f43, f44, f45, f46, f47, f48, f49, f50, f51, f52, f53, f54, f55, f56);
}

// Arity 57: reference-member detection for the serializer backstop.
template <typename T>
constexpr auto has_reference_field_tag(T&, std::integral_constant<std::size_t, 57>)
    -> decltype([](T& u) {
           auto& [f0, f1, f2, f3, f4, f5, f6, f7, f8, f9, f10, f11, f12, f13, f14, f15, f16, f17, f18, f19, f20, f21, f22, f23, f24, f25, f26, f27, f28, f29, f30, f31, f32, f33, f34, f35, f36, f37, f38, f39, f40, f41, f42, f43, f44, f45, f46, f47, f48, f49, f50, f51, f52, f53, f54, f55, f56] = u;
           return std::bool_constant<(std::is_reference_v<decltype(f0)> || std::is_reference_v<decltype(f1)> || std::is_reference_v<decltype(f2)> || std::is_reference_v<decltype(f3)> || std::is_reference_v<decltype(f4)> || std::is_reference_v<decltype(f5)> || std::is_reference_v<decltype(f6)> || std::is_reference_v<decltype(f7)> || std::is_reference_v<decltype(f8)> || std::is_reference_v<decltype(f9)> || std::is_reference_v<decltype(f10)> || std::is_reference_v<decltype(f11)> || std::is_reference_v<decltype(f12)> || std::is_reference_v<decltype(f13)> || std::is_reference_v<decltype(f14)> || std::is_reference_v<decltype(f15)> || std::is_reference_v<decltype(f16)> || std::is_reference_v<decltype(f17)> || std::is_reference_v<decltype(f18)> || std::is_reference_v<decltype(f19)> || std::is_reference_v<decltype(f20)> || std::is_reference_v<decltype(f21)> || std::is_reference_v<decltype(f22)> || std::is_reference_v<decltype(f23)> || std::is_reference_v<decltype(f24)> || std::is_reference_v<decltype(f25)> || std::is_reference_v<decltype(f26)> || std::is_reference_v<decltype(f27)> || std::is_reference_v<decltype(f28)> || std::is_reference_v<decltype(f29)> || std::is_reference_v<decltype(f30)> || std::is_reference_v<decltype(f31)> || std::is_reference_v<decltype(f32)> || std::is_reference_v<decltype(f33)> || std::is_reference_v<decltype(f34)> || std::is_reference_v<decltype(f35)> || std::is_reference_v<decltype(f36)> || std::is_reference_v<decltype(f37)> || std::is_reference_v<decltype(f38)> || std::is_reference_v<decltype(f39)> || std::is_reference_v<decltype(f40)> || std::is_reference_v<decltype(f41)> || std::is_reference_v<decltype(f42)> || std::is_reference_v<decltype(f43)> || std::is_reference_v<decltype(f44)> || std::is_reference_v<decltype(f45)> || std::is_reference_v<decltype(f46)> || std::is_reference_v<decltype(f47)> || std::is_reference_v<decltype(f48)> || std::is_reference_v<decltype(f49)> || std::is_reference_v<decltype(f50)> || std::is_reference_v<decltype(f51)> || std::is_reference_v<decltype(f52)> || std::is_reference_v<decltype(f53)> || std::is_reference_v<decltype(f54)> || std::is_reference_v<decltype(f55)> || std::is_reference_v<decltype(f56)>)>{};
       }(std::declval<T&>()));

// Arity 58: decompose into a tuple of references.
template <typename T>
constexpr auto to_tuple_impl(T&& obj, std::integral_constant<std::size_t, 58>) {
    auto&& [f0, f1, f2, f3, f4, f5, f6, f7, f8, f9, f10, f11, f12, f13, f14, f15, f16, f17, f18, f19, f20, f21, f22, f23, f24, f25, f26, f27, f28, f29, f30, f31, f32, f33, f34, f35, f36, f37, f38, f39, f40, f41, f42, f43, f44, f45, f46, f47, f48, f49, f50, f51, f52, f53, f54, f55, f56, f57] = std::forward<T>(obj);
    return std::forward_as_tuple(f0, f1, f2, f3, f4, f5, f6, f7, f8, f9, f10, f11, f12, f13, f14, f15, f16, f17, f18, f19, f20, f21, f22, f23, f24, f25, f26, f27, f28, f29, f30, f31, f32, f33, f34, f35, f36, f37, f38, f39, f40, f41, f42, f43, f44, f45, f46, f47, f48, f49, f50, f51, f52, f53, f54, f55, f56, f57);
}

// Arity 58: reference-member detection for the serializer backstop.
template <typename T>
constexpr auto has_reference_field_tag(T&, std::integral_constant<std::size_t, 58>)
    -> decltype([](T& u) {
           auto& [f0, f1, f2, f3, f4, f5, f6, f7, f8, f9, f10, f11, f12, f13, f14, f15, f16, f17, f18, f19, f20, f21, f22, f23, f24, f25, f26, f27, f28, f29, f30, f31, f32, f33, f34, f35, f36, f37, f38, f39, f40, f41, f42, f43, f44, f45, f46, f47, f48, f49, f50, f51, f52, f53, f54, f55, f56, f57] = u;
           return std::bool_constant<(std::is_reference_v<decltype(f0)> || std::is_reference_v<decltype(f1)> || std::is_reference_v<decltype(f2)> || std::is_reference_v<decltype(f3)> || std::is_reference_v<decltype(f4)> || std::is_reference_v<decltype(f5)> || std::is_reference_v<decltype(f6)> || std::is_reference_v<decltype(f7)> || std::is_reference_v<decltype(f8)> || std::is_reference_v<decltype(f9)> || std::is_reference_v<decltype(f10)> || std::is_reference_v<decltype(f11)> || std::is_reference_v<decltype(f12)> || std::is_reference_v<decltype(f13)> || std::is_reference_v<decltype(f14)> || std::is_reference_v<decltype(f15)> || std::is_reference_v<decltype(f16)> || std::is_reference_v<decltype(f17)> || std::is_reference_v<decltype(f18)> || std::is_reference_v<decltype(f19)> || std::is_reference_v<decltype(f20)> || std::is_reference_v<decltype(f21)> || std::is_reference_v<decltype(f22)> || std::is_reference_v<decltype(f23)> || std::is_reference_v<decltype(f24)> || std::is_reference_v<decltype(f25)> || std::is_reference_v<decltype(f26)> || std::is_reference_v<decltype(f27)> || std::is_reference_v<decltype(f28)> || std::is_reference_v<decltype(f29)> || std::is_reference_v<decltype(f30)> || std::is_reference_v<decltype(f31)> || std::is_reference_v<decltype(f32)> || std::is_reference_v<decltype(f33)> || std::is_reference_v<decltype(f34)> || std::is_reference_v<decltype(f35)> || std::is_reference_v<decltype(f36)> || std::is_reference_v<decltype(f37)> || std::is_reference_v<decltype(f38)> || std::is_reference_v<decltype(f39)> || std::is_reference_v<decltype(f40)> || std::is_reference_v<decltype(f41)> || std::is_reference_v<decltype(f42)> || std::is_reference_v<decltype(f43)> || std::is_reference_v<decltype(f44)> || std::is_reference_v<decltype(f45)> || std::is_reference_v<decltype(f46)> || std::is_reference_v<decltype(f47)> || std::is_reference_v<decltype(f48)> || std::is_reference_v<decltype(f49)> || std::is_reference_v<decltype(f50)> || std::is_reference_v<decltype(f51)> || std::is_reference_v<decltype(f52)> || std::is_reference_v<decltype(f53)> || std::is_reference_v<decltype(f54)> || std::is_reference_v<decltype(f55)> || std::is_reference_v<decltype(f56)> || std::is_reference_v<decltype(f57)>)>{};
       }(std::declval<T&>()));

// Arity 59: decompose into a tuple of references.
template <typename T>
constexpr auto to_tuple_impl(T&& obj, std::integral_constant<std::size_t, 59>) {
    auto&& [f0, f1, f2, f3, f4, f5, f6, f7, f8, f9, f10, f11, f12, f13, f14, f15, f16, f17, f18, f19, f20, f21, f22, f23, f24, f25, f26, f27, f28, f29, f30, f31, f32, f33, f34, f35, f36, f37, f38, f39, f40, f41, f42, f43, f44, f45, f46, f47, f48, f49, f50, f51, f52, f53, f54, f55, f56, f57, f58] = std::forward<T>(obj);
    return std::forward_as_tuple(f0, f1, f2, f3, f4, f5, f6, f7, f8, f9, f10, f11, f12, f13, f14, f15, f16, f17, f18, f19, f20, f21, f22, f23, f24, f25, f26, f27, f28, f29, f30, f31, f32, f33, f34, f35, f36, f37, f38, f39, f40, f41, f42, f43, f44, f45, f46, f47, f48, f49, f50, f51, f52, f53, f54, f55, f56, f57, f58);
}

// Arity 59: reference-member detection for the serializer backstop.
template <typename T>
constexpr auto has_reference_field_tag(T&, std::integral_constant<std::size_t, 59>)
    -> decltype([](T& u) {
           auto& [f0, f1, f2, f3, f4, f5, f6, f7, f8, f9, f10, f11, f12, f13, f14, f15, f16, f17, f18, f19, f20, f21, f22, f23, f24, f25, f26, f27, f28, f29, f30, f31, f32, f33, f34, f35, f36, f37, f38, f39, f40, f41, f42, f43, f44, f45, f46, f47, f48, f49, f50, f51, f52, f53, f54, f55, f56, f57, f58] = u;
           return std::bool_constant<(std::is_reference_v<decltype(f0)> || std::is_reference_v<decltype(f1)> || std::is_reference_v<decltype(f2)> || std::is_reference_v<decltype(f3)> || std::is_reference_v<decltype(f4)> || std::is_reference_v<decltype(f5)> || std::is_reference_v<decltype(f6)> || std::is_reference_v<decltype(f7)> || std::is_reference_v<decltype(f8)> || std::is_reference_v<decltype(f9)> || std::is_reference_v<decltype(f10)> || std::is_reference_v<decltype(f11)> || std::is_reference_v<decltype(f12)> || std::is_reference_v<decltype(f13)> || std::is_reference_v<decltype(f14)> || std::is_reference_v<decltype(f15)> || std::is_reference_v<decltype(f16)> || std::is_reference_v<decltype(f17)> || std::is_reference_v<decltype(f18)> || std::is_reference_v<decltype(f19)> || std::is_reference_v<decltype(f20)> || std::is_reference_v<decltype(f21)> || std::is_reference_v<decltype(f22)> || std::is_reference_v<decltype(f23)> || std::is_reference_v<decltype(f24)> || std::is_reference_v<decltype(f25)> || std::is_reference_v<decltype(f26)> || std::is_reference_v<decltype(f27)> || std::is_reference_v<decltype(f28)> || std::is_reference_v<decltype(f29)> || std::is_reference_v<decltype(f30)> || std::is_reference_v<decltype(f31)> || std::is_reference_v<decltype(f32)> || std::is_reference_v<decltype(f33)> || std::is_reference_v<decltype(f34)> || std::is_reference_v<decltype(f35)> || std::is_reference_v<decltype(f36)> || std::is_reference_v<decltype(f37)> || std::is_reference_v<decltype(f38)> || std::is_reference_v<decltype(f39)> || std::is_reference_v<decltype(f40)> || std::is_reference_v<decltype(f41)> || std::is_reference_v<decltype(f42)> || std::is_reference_v<decltype(f43)> || std::is_reference_v<decltype(f44)> || std::is_reference_v<decltype(f45)> || std::is_reference_v<decltype(f46)> || std::is_reference_v<decltype(f47)> || std::is_reference_v<decltype(f48)> || std::is_reference_v<decltype(f49)> || std::is_reference_v<decltype(f50)> || std::is_reference_v<decltype(f51)> || std::is_reference_v<decltype(f52)> || std::is_reference_v<decltype(f53)> || std::is_reference_v<decltype(f54)> || std::is_reference_v<decltype(f55)> || std::is_reference_v<decltype(f56)> || std::is_reference_v<decltype(f57)> || std::is_reference_v<decltype(f58)>)>{};
       }(std::declval<T&>()));

// Arity 60: decompose into a tuple of references.
template <typename T>
constexpr auto to_tuple_impl(T&& obj, std::integral_constant<std::size_t, 60>) {
    auto&& [f0, f1, f2, f3, f4, f5, f6, f7, f8, f9, f10, f11, f12, f13, f14, f15, f16, f17, f18, f19, f20, f21, f22, f23, f24, f25, f26, f27, f28, f29, f30, f31, f32, f33, f34, f35, f36, f37, f38, f39, f40, f41, f42, f43, f44, f45, f46, f47, f48, f49, f50, f51, f52, f53, f54, f55, f56, f57, f58, f59] = std::forward<T>(obj);
    return std::forward_as_tuple(f0, f1, f2, f3, f4, f5, f6, f7, f8, f9, f10, f11, f12, f13, f14, f15, f16, f17, f18, f19, f20, f21, f22, f23, f24, f25, f26, f27, f28, f29, f30, f31, f32, f33, f34, f35, f36, f37, f38, f39, f40, f41, f42, f43, f44, f45, f46, f47, f48, f49, f50, f51, f52, f53, f54, f55, f56, f57, f58, f59);
}

// Arity 60: reference-member detection for the serializer backstop.
template <typename T>
constexpr auto has_reference_field_tag(T&, std::integral_constant<std::size_t, 60>)
    -> decltype([](T& u) {
           auto& [f0, f1, f2, f3, f4, f5, f6, f7, f8, f9, f10, f11, f12, f13, f14, f15, f16, f17, f18, f19, f20, f21, f22, f23, f24, f25, f26, f27, f28, f29, f30, f31, f32, f33, f34, f35, f36, f37, f38, f39, f40, f41, f42, f43, f44, f45, f46, f47, f48, f49, f50, f51, f52, f53, f54, f55, f56, f57, f58, f59] = u;
           return std::bool_constant<(std::is_reference_v<decltype(f0)> || std::is_reference_v<decltype(f1)> || std::is_reference_v<decltype(f2)> || std::is_reference_v<decltype(f3)> || std::is_reference_v<decltype(f4)> || std::is_reference_v<decltype(f5)> || std::is_reference_v<decltype(f6)> || std::is_reference_v<decltype(f7)> || std::is_reference_v<decltype(f8)> || std::is_reference_v<decltype(f9)> || std::is_reference_v<decltype(f10)> || std::is_reference_v<decltype(f11)> || std::is_reference_v<decltype(f12)> || std::is_reference_v<decltype(f13)> || std::is_reference_v<decltype(f14)> || std::is_reference_v<decltype(f15)> || std::is_reference_v<decltype(f16)> || std::is_reference_v<decltype(f17)> || std::is_reference_v<decltype(f18)> || std::is_reference_v<decltype(f19)> || std::is_reference_v<decltype(f20)> || std::is_reference_v<decltype(f21)> || std::is_reference_v<decltype(f22)> || std::is_reference_v<decltype(f23)> || std::is_reference_v<decltype(f24)> || std::is_reference_v<decltype(f25)> || std::is_reference_v<decltype(f26)> || std::is_reference_v<decltype(f27)> || std::is_reference_v<decltype(f28)> || std::is_reference_v<decltype(f29)> || std::is_reference_v<decltype(f30)> || std::is_reference_v<decltype(f31)> || std::is_reference_v<decltype(f32)> || std::is_reference_v<decltype(f33)> || std::is_reference_v<decltype(f34)> || std::is_reference_v<decltype(f35)> || std::is_reference_v<decltype(f36)> || std::is_reference_v<decltype(f37)> || std::is_reference_v<decltype(f38)> || std::is_reference_v<decltype(f39)> || std::is_reference_v<decltype(f40)> || std::is_reference_v<decltype(f41)> || std::is_reference_v<decltype(f42)> || std::is_reference_v<decltype(f43)> || std::is_reference_v<decltype(f44)> || std::is_reference_v<decltype(f45)> || std::is_reference_v<decltype(f46)> || std::is_reference_v<decltype(f47)> || std::is_reference_v<decltype(f48)> || std::is_reference_v<decltype(f49)> || std::is_reference_v<decltype(f50)> || std::is_reference_v<decltype(f51)> || std::is_reference_v<decltype(f52)> || std::is_reference_v<decltype(f53)> || std::is_reference_v<decltype(f54)> || std::is_reference_v<decltype(f55)> || std::is_reference_v<decltype(f56)> || std::is_reference_v<decltype(f57)> || std::is_reference_v<decltype(f58)> || std::is_reference_v<decltype(f59)>)>{};
       }(std::declval<T&>()));

// Arity 61: decompose into a tuple of references.
template <typename T>
constexpr auto to_tuple_impl(T&& obj, std::integral_constant<std::size_t, 61>) {
    auto&& [f0, f1, f2, f3, f4, f5, f6, f7, f8, f9, f10, f11, f12, f13, f14, f15, f16, f17, f18, f19, f20, f21, f22, f23, f24, f25, f26, f27, f28, f29, f30, f31, f32, f33, f34, f35, f36, f37, f38, f39, f40, f41, f42, f43, f44, f45, f46, f47, f48, f49, f50, f51, f52, f53, f54, f55, f56, f57, f58, f59, f60] = std::forward<T>(obj);
    return std::forward_as_tuple(f0, f1, f2, f3, f4, f5, f6, f7, f8, f9, f10, f11, f12, f13, f14, f15, f16, f17, f18, f19, f20, f21, f22, f23, f24, f25, f26, f27, f28, f29, f30, f31, f32, f33, f34, f35, f36, f37, f38, f39, f40, f41, f42, f43, f44, f45, f46, f47, f48, f49, f50, f51, f52, f53, f54, f55, f56, f57, f58, f59, f60);
}

// Arity 61: reference-member detection for the serializer backstop.
template <typename T>
constexpr auto has_reference_field_tag(T&, std::integral_constant<std::size_t, 61>)
    -> decltype([](T& u) {
           auto& [f0, f1, f2, f3, f4, f5, f6, f7, f8, f9, f10, f11, f12, f13, f14, f15, f16, f17, f18, f19, f20, f21, f22, f23, f24, f25, f26, f27, f28, f29, f30, f31, f32, f33, f34, f35, f36, f37, f38, f39, f40, f41, f42, f43, f44, f45, f46, f47, f48, f49, f50, f51, f52, f53, f54, f55, f56, f57, f58, f59, f60] = u;
           return std::bool_constant<(std::is_reference_v<decltype(f0)> || std::is_reference_v<decltype(f1)> || std::is_reference_v<decltype(f2)> || std::is_reference_v<decltype(f3)> || std::is_reference_v<decltype(f4)> || std::is_reference_v<decltype(f5)> || std::is_reference_v<decltype(f6)> || std::is_reference_v<decltype(f7)> || std::is_reference_v<decltype(f8)> || std::is_reference_v<decltype(f9)> || std::is_reference_v<decltype(f10)> || std::is_reference_v<decltype(f11)> || std::is_reference_v<decltype(f12)> || std::is_reference_v<decltype(f13)> || std::is_reference_v<decltype(f14)> || std::is_reference_v<decltype(f15)> || std::is_reference_v<decltype(f16)> || std::is_reference_v<decltype(f17)> || std::is_reference_v<decltype(f18)> || std::is_reference_v<decltype(f19)> || std::is_reference_v<decltype(f20)> || std::is_reference_v<decltype(f21)> || std::is_reference_v<decltype(f22)> || std::is_reference_v<decltype(f23)> || std::is_reference_v<decltype(f24)> || std::is_reference_v<decltype(f25)> || std::is_reference_v<decltype(f26)> || std::is_reference_v<decltype(f27)> || std::is_reference_v<decltype(f28)> || std::is_reference_v<decltype(f29)> || std::is_reference_v<decltype(f30)> || std::is_reference_v<decltype(f31)> || std::is_reference_v<decltype(f32)> || std::is_reference_v<decltype(f33)> || std::is_reference_v<decltype(f34)> || std::is_reference_v<decltype(f35)> || std::is_reference_v<decltype(f36)> || std::is_reference_v<decltype(f37)> || std::is_reference_v<decltype(f38)> || std::is_reference_v<decltype(f39)> || std::is_reference_v<decltype(f40)> || std::is_reference_v<decltype(f41)> || std::is_reference_v<decltype(f42)> || std::is_reference_v<decltype(f43)> || std::is_reference_v<decltype(f44)> || std::is_reference_v<decltype(f45)> || std::is_reference_v<decltype(f46)> || std::is_reference_v<decltype(f47)> || std::is_reference_v<decltype(f48)> || std::is_reference_v<decltype(f49)> || std::is_reference_v<decltype(f50)> || std::is_reference_v<decltype(f51)> || std::is_reference_v<decltype(f52)> || std::is_reference_v<decltype(f53)> || std::is_reference_v<decltype(f54)> || std::is_reference_v<decltype(f55)> || std::is_reference_v<decltype(f56)> || std::is_reference_v<decltype(f57)> || std::is_reference_v<decltype(f58)> || std::is_reference_v<decltype(f59)> || std::is_reference_v<decltype(f60)>)>{};
       }(std::declval<T&>()));

// Arity 62: decompose into a tuple of references.
template <typename T>
constexpr auto to_tuple_impl(T&& obj, std::integral_constant<std::size_t, 62>) {
    auto&& [f0, f1, f2, f3, f4, f5, f6, f7, f8, f9, f10, f11, f12, f13, f14, f15, f16, f17, f18, f19, f20, f21, f22, f23, f24, f25, f26, f27, f28, f29, f30, f31, f32, f33, f34, f35, f36, f37, f38, f39, f40, f41, f42, f43, f44, f45, f46, f47, f48, f49, f50, f51, f52, f53, f54, f55, f56, f57, f58, f59, f60, f61] = std::forward<T>(obj);
    return std::forward_as_tuple(f0, f1, f2, f3, f4, f5, f6, f7, f8, f9, f10, f11, f12, f13, f14, f15, f16, f17, f18, f19, f20, f21, f22, f23, f24, f25, f26, f27, f28, f29, f30, f31, f32, f33, f34, f35, f36, f37, f38, f39, f40, f41, f42, f43, f44, f45, f46, f47, f48, f49, f50, f51, f52, f53, f54, f55, f56, f57, f58, f59, f60, f61);
}

// Arity 62: reference-member detection for the serializer backstop.
template <typename T>
constexpr auto has_reference_field_tag(T&, std::integral_constant<std::size_t, 62>)
    -> decltype([](T& u) {
           auto& [f0, f1, f2, f3, f4, f5, f6, f7, f8, f9, f10, f11, f12, f13, f14, f15, f16, f17, f18, f19, f20, f21, f22, f23, f24, f25, f26, f27, f28, f29, f30, f31, f32, f33, f34, f35, f36, f37, f38, f39, f40, f41, f42, f43, f44, f45, f46, f47, f48, f49, f50, f51, f52, f53, f54, f55, f56, f57, f58, f59, f60, f61] = u;
           return std::bool_constant<(std::is_reference_v<decltype(f0)> || std::is_reference_v<decltype(f1)> || std::is_reference_v<decltype(f2)> || std::is_reference_v<decltype(f3)> || std::is_reference_v<decltype(f4)> || std::is_reference_v<decltype(f5)> || std::is_reference_v<decltype(f6)> || std::is_reference_v<decltype(f7)> || std::is_reference_v<decltype(f8)> || std::is_reference_v<decltype(f9)> || std::is_reference_v<decltype(f10)> || std::is_reference_v<decltype(f11)> || std::is_reference_v<decltype(f12)> || std::is_reference_v<decltype(f13)> || std::is_reference_v<decltype(f14)> || std::is_reference_v<decltype(f15)> || std::is_reference_v<decltype(f16)> || std::is_reference_v<decltype(f17)> || std::is_reference_v<decltype(f18)> || std::is_reference_v<decltype(f19)> || std::is_reference_v<decltype(f20)> || std::is_reference_v<decltype(f21)> || std::is_reference_v<decltype(f22)> || std::is_reference_v<decltype(f23)> || std::is_reference_v<decltype(f24)> || std::is_reference_v<decltype(f25)> || std::is_reference_v<decltype(f26)> || std::is_reference_v<decltype(f27)> || std::is_reference_v<decltype(f28)> || std::is_reference_v<decltype(f29)> || std::is_reference_v<decltype(f30)> || std::is_reference_v<decltype(f31)> || std::is_reference_v<decltype(f32)> || std::is_reference_v<decltype(f33)> || std::is_reference_v<decltype(f34)> || std::is_reference_v<decltype(f35)> || std::is_reference_v<decltype(f36)> || std::is_reference_v<decltype(f37)> || std::is_reference_v<decltype(f38)> || std::is_reference_v<decltype(f39)> || std::is_reference_v<decltype(f40)> || std::is_reference_v<decltype(f41)> || std::is_reference_v<decltype(f42)> || std::is_reference_v<decltype(f43)> || std::is_reference_v<decltype(f44)> || std::is_reference_v<decltype(f45)> || std::is_reference_v<decltype(f46)> || std::is_reference_v<decltype(f47)> || std::is_reference_v<decltype(f48)> || std::is_reference_v<decltype(f49)> || std::is_reference_v<decltype(f50)> || std::is_reference_v<decltype(f51)> || std::is_reference_v<decltype(f52)> || std::is_reference_v<decltype(f53)> || std::is_reference_v<decltype(f54)> || std::is_reference_v<decltype(f55)> || std::is_reference_v<decltype(f56)> || std::is_reference_v<decltype(f57)> || std::is_reference_v<decltype(f58)> || std::is_reference_v<decltype(f59)> || std::is_reference_v<decltype(f60)> || std::is_reference_v<decltype(f61)>)>{};
       }(std::declval<T&>()));

// Arity 63: decompose into a tuple of references.
template <typename T>
constexpr auto to_tuple_impl(T&& obj, std::integral_constant<std::size_t, 63>) {
    auto&& [f0, f1, f2, f3, f4, f5, f6, f7, f8, f9, f10, f11, f12, f13, f14, f15, f16, f17, f18, f19, f20, f21, f22, f23, f24, f25, f26, f27, f28, f29, f30, f31, f32, f33, f34, f35, f36, f37, f38, f39, f40, f41, f42, f43, f44, f45, f46, f47, f48, f49, f50, f51, f52, f53, f54, f55, f56, f57, f58, f59, f60, f61, f62] = std::forward<T>(obj);
    return std::forward_as_tuple(f0, f1, f2, f3, f4, f5, f6, f7, f8, f9, f10, f11, f12, f13, f14, f15, f16, f17, f18, f19, f20, f21, f22, f23, f24, f25, f26, f27, f28, f29, f30, f31, f32, f33, f34, f35, f36, f37, f38, f39, f40, f41, f42, f43, f44, f45, f46, f47, f48, f49, f50, f51, f52, f53, f54, f55, f56, f57, f58, f59, f60, f61, f62);
}

// Arity 63: reference-member detection for the serializer backstop.
template <typename T>
constexpr auto has_reference_field_tag(T&, std::integral_constant<std::size_t, 63>)
    -> decltype([](T& u) {
           auto& [f0, f1, f2, f3, f4, f5, f6, f7, f8, f9, f10, f11, f12, f13, f14, f15, f16, f17, f18, f19, f20, f21, f22, f23, f24, f25, f26, f27, f28, f29, f30, f31, f32, f33, f34, f35, f36, f37, f38, f39, f40, f41, f42, f43, f44, f45, f46, f47, f48, f49, f50, f51, f52, f53, f54, f55, f56, f57, f58, f59, f60, f61, f62] = u;
           return std::bool_constant<(std::is_reference_v<decltype(f0)> || std::is_reference_v<decltype(f1)> || std::is_reference_v<decltype(f2)> || std::is_reference_v<decltype(f3)> || std::is_reference_v<decltype(f4)> || std::is_reference_v<decltype(f5)> || std::is_reference_v<decltype(f6)> || std::is_reference_v<decltype(f7)> || std::is_reference_v<decltype(f8)> || std::is_reference_v<decltype(f9)> || std::is_reference_v<decltype(f10)> || std::is_reference_v<decltype(f11)> || std::is_reference_v<decltype(f12)> || std::is_reference_v<decltype(f13)> || std::is_reference_v<decltype(f14)> || std::is_reference_v<decltype(f15)> || std::is_reference_v<decltype(f16)> || std::is_reference_v<decltype(f17)> || std::is_reference_v<decltype(f18)> || std::is_reference_v<decltype(f19)> || std::is_reference_v<decltype(f20)> || std::is_reference_v<decltype(f21)> || std::is_reference_v<decltype(f22)> || std::is_reference_v<decltype(f23)> || std::is_reference_v<decltype(f24)> || std::is_reference_v<decltype(f25)> || std::is_reference_v<decltype(f26)> || std::is_reference_v<decltype(f27)> || std::is_reference_v<decltype(f28)> || std::is_reference_v<decltype(f29)> || std::is_reference_v<decltype(f30)> || std::is_reference_v<decltype(f31)> || std::is_reference_v<decltype(f32)> || std::is_reference_v<decltype(f33)> || std::is_reference_v<decltype(f34)> || std::is_reference_v<decltype(f35)> || std::is_reference_v<decltype(f36)> || std::is_reference_v<decltype(f37)> || std::is_reference_v<decltype(f38)> || std::is_reference_v<decltype(f39)> || std::is_reference_v<decltype(f40)> || std::is_reference_v<decltype(f41)> || std::is_reference_v<decltype(f42)> || std::is_reference_v<decltype(f43)> || std::is_reference_v<decltype(f44)> || std::is_reference_v<decltype(f45)> || std::is_reference_v<decltype(f46)> || std::is_reference_v<decltype(f47)> || std::is_reference_v<decltype(f48)> || std::is_reference_v<decltype(f49)> || std::is_reference_v<decltype(f50)> || std::is_reference_v<decltype(f51)> || std::is_reference_v<decltype(f52)> || std::is_reference_v<decltype(f53)> || std::is_reference_v<decltype(f54)> || std::is_reference_v<decltype(f55)> || std::is_reference_v<decltype(f56)> || std::is_reference_v<decltype(f57)> || std::is_reference_v<decltype(f58)> || std::is_reference_v<decltype(f59)> || std::is_reference_v<decltype(f60)> || std::is_reference_v<decltype(f61)> || std::is_reference_v<decltype(f62)>)>{};
       }(std::declval<T&>()));

// Arity 64: decompose into a tuple of references.
template <typename T>
constexpr auto to_tuple_impl(T&& obj, std::integral_constant<std::size_t, 64>) {
    auto&& [f0, f1, f2, f3, f4, f5, f6, f7, f8, f9, f10, f11, f12, f13, f14, f15, f16, f17, f18, f19, f20, f21, f22, f23, f24, f25, f26, f27, f28, f29, f30, f31, f32, f33, f34, f35, f36, f37, f38, f39, f40, f41, f42, f43, f44, f45, f46, f47, f48, f49, f50, f51, f52, f53, f54, f55, f56, f57, f58, f59, f60, f61, f62, f63] = std::forward<T>(obj);
    return std::forward_as_tuple(f0, f1, f2, f3, f4, f5, f6, f7, f8, f9, f10, f11, f12, f13, f14, f15, f16, f17, f18, f19, f20, f21, f22, f23, f24, f25, f26, f27, f28, f29, f30, f31, f32, f33, f34, f35, f36, f37, f38, f39, f40, f41, f42, f43, f44, f45, f46, f47, f48, f49, f50, f51, f52, f53, f54, f55, f56, f57, f58, f59, f60, f61, f62, f63);
}

// Arity 64: reference-member detection for the serializer backstop.
template <typename T>
constexpr auto has_reference_field_tag(T&, std::integral_constant<std::size_t, 64>)
    -> decltype([](T& u) {
           auto& [f0, f1, f2, f3, f4, f5, f6, f7, f8, f9, f10, f11, f12, f13, f14, f15, f16, f17, f18, f19, f20, f21, f22, f23, f24, f25, f26, f27, f28, f29, f30, f31, f32, f33, f34, f35, f36, f37, f38, f39, f40, f41, f42, f43, f44, f45, f46, f47, f48, f49, f50, f51, f52, f53, f54, f55, f56, f57, f58, f59, f60, f61, f62, f63] = u;
           return std::bool_constant<(std::is_reference_v<decltype(f0)> || std::is_reference_v<decltype(f1)> || std::is_reference_v<decltype(f2)> || std::is_reference_v<decltype(f3)> || std::is_reference_v<decltype(f4)> || std::is_reference_v<decltype(f5)> || std::is_reference_v<decltype(f6)> || std::is_reference_v<decltype(f7)> || std::is_reference_v<decltype(f8)> || std::is_reference_v<decltype(f9)> || std::is_reference_v<decltype(f10)> || std::is_reference_v<decltype(f11)> || std::is_reference_v<decltype(f12)> || std::is_reference_v<decltype(f13)> || std::is_reference_v<decltype(f14)> || std::is_reference_v<decltype(f15)> || std::is_reference_v<decltype(f16)> || std::is_reference_v<decltype(f17)> || std::is_reference_v<decltype(f18)> || std::is_reference_v<decltype(f19)> || std::is_reference_v<decltype(f20)> || std::is_reference_v<decltype(f21)> || std::is_reference_v<decltype(f22)> || std::is_reference_v<decltype(f23)> || std::is_reference_v<decltype(f24)> || std::is_reference_v<decltype(f25)> || std::is_reference_v<decltype(f26)> || std::is_reference_v<decltype(f27)> || std::is_reference_v<decltype(f28)> || std::is_reference_v<decltype(f29)> || std::is_reference_v<decltype(f30)> || std::is_reference_v<decltype(f31)> || std::is_reference_v<decltype(f32)> || std::is_reference_v<decltype(f33)> || std::is_reference_v<decltype(f34)> || std::is_reference_v<decltype(f35)> || std::is_reference_v<decltype(f36)> || std::is_reference_v<decltype(f37)> || std::is_reference_v<decltype(f38)> || std::is_reference_v<decltype(f39)> || std::is_reference_v<decltype(f40)> || std::is_reference_v<decltype(f41)> || std::is_reference_v<decltype(f42)> || std::is_reference_v<decltype(f43)> || std::is_reference_v<decltype(f44)> || std::is_reference_v<decltype(f45)> || std::is_reference_v<decltype(f46)> || std::is_reference_v<decltype(f47)> || std::is_reference_v<decltype(f48)> || std::is_reference_v<decltype(f49)> || std::is_reference_v<decltype(f50)> || std::is_reference_v<decltype(f51)> || std::is_reference_v<decltype(f52)> || std::is_reference_v<decltype(f53)> || std::is_reference_v<decltype(f54)> || std::is_reference_v<decltype(f55)> || std::is_reference_v<decltype(f56)> || std::is_reference_v<decltype(f57)> || std::is_reference_v<decltype(f58)> || std::is_reference_v<decltype(f59)> || std::is_reference_v<decltype(f60)> || std::is_reference_v<decltype(f61)> || std::is_reference_v<decltype(f62)> || std::is_reference_v<decltype(f63)>)>{};
       }(std::declval<T&>()));

}  // namespace simulnetx::reflect::detail
