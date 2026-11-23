#pragma once

#include <concepts>
#include <cstddef>
#include <tuple>
#include <type_traits>
#include <utility>

#include "simulnetx/detail/reflect_generated.hxx"

namespace simulnetx::reflect {

// Aggregate reflection, PFR-style, written for this library: no macros, no
// dependencies. Only aggregates are supported (see reflectable); the field
// count comes from a constructibility probe and field access from structured
// bindings, so plain user structs "just work" with no boilerplate.
//
// Two things this engine deliberately does NOT do:
//  - Field names: there is no portable way to get them from an index in
//    C++20 (they need member pointers, which need the names). Names are a
//    future extension, not a v0.4.0 feature; nothing in the library depends
//    on them.
//  - C-style array members: the arity probe cannot count them (brace
//    elision flattens them in braced init, and no single expression can
//    initialize an array member in paren init), so reflecting a struct with
//    one fails at compile time with a structured-binding count mismatch.
//    Use std::array instead, which is a single class-type member and works.
//
// The arity machinery lives in detail so that a future C++26 std::meta
// engine can replace it without changing this public API.

namespace detail {

// Probe types. any_value converts to any value type (as a prvalue); it
// cannot initialize a reference member, because a prvalue does not bind to
// a non-const lvalue reference. any_lvalue converts to any lvalue reference
// and CAN initialize reference members. Comparing the two counts is how
// reference members are detected (see has_reference_field).
struct any_value {
    template <typename T>
    constexpr operator T() const noexcept;
};

struct any_lvalue {
    template <typename T>
    constexpr operator T&() const noexcept;
};

// Field count probe: direct (paren) initialization, which - unlike braced
// init - does not apply brace elision, so nested aggregates count as one
// field each. std::is_constructible uses direct init. The count is the
// largest N in [0, 65] for which T is constructible from N probe arguments;
// 65 means "more than 64 fields" and is never a valid count.
//
// Caveat: N == 1 is polluted by the copy/move constructor (any_value
// converts to T itself), so a 1-count on an empty type really means 0
// fields. That is corrected below.
template <typename T, std::size_t... Is>
constexpr bool paren_constructible(std::index_sequence<Is...>) {
    return std::is_constructible_v<T, decltype((static_cast<void>(Is), any_value{}))...>;
}

template <typename T, std::size_t... Is>
consteval std::size_t count_paren_impl(std::index_sequence<Is...>) {
    constexpr bool checks[] = {paren_constructible<T>(std::make_index_sequence<Is>{})...};
    std::size_t result = 0;
    for (std::size_t i = 0; i < sizeof...(Is); ++i) {
        if (checks[i]) {
            result = i;
        }
    }
    if (result == 1 && std::is_empty_v<T>) {
        return 0;
    }
    return result;
}

template <typename T>
consteval std::size_t count_paren() {
    return count_paren_impl<T>(std::make_index_sequence<66>{});
}

// Braced-init count, for reference detection only. Braced init has no
// copy-ctor pollution, and elision inflates both probe counts equally, so
// the comparison below is meaningful even though the absolute braced count
// is not the field count (arrays and nested aggregates elide).
template <typename Any, typename T, std::size_t... Is>
consteval bool braced_constructible(std::index_sequence<Is...>) {
    return requires { T{(static_cast<void>(Is), Any{})...}; };
}

template <typename Any, typename T, std::size_t... Is>
consteval std::size_t count_braced_impl(std::index_sequence<Is...>) {
    constexpr bool checks[] = {braced_constructible<Any, T>(std::make_index_sequence<Is>{})...};
    std::size_t result = 0;
    for (std::size_t i = 0; i < sizeof...(Is); ++i) {
        if (checks[i]) {
            result = i;
        }
    }
    return result;
}

template <typename Any, typename T>
consteval std::size_t count_braced() {
    return count_braced_impl<Any, T>(std::make_index_sequence<66>{});
}

// True iff T has a non-const reference member. any_value cannot initialize
// one (and cannot value-initialize one either), so every braced count with
// any_value fails and that count is 0; any_lvalue can, so its count is the
// (elision-inflated) field count. For reference-free structs the two counts
// are equal: move-only members only shrink the any_lvalue count, never grow
// it. Const reference members are NOT detected here (a prvalue binds to a
// const reference); they are caught by has_reference_field_decltype_v, the
// serializer backstop.
template <typename T>
consteval bool has_reference_field() {
    return count_braced<any_lvalue, T>() > count_braced<any_value, T>();
}

// Decltype-based reference detection (generated per-arity machinery): the
// binding of a reference member has reference type, a value member's does
// not. This catches const references too, but it instantiates a structured
// binding with the probed count, so it is a HARD ERROR for types whose
// count is wrong (C-array members) - it must only be used at use sites
// (the serializer's static_assert), never inside the reflectable concept,
// which has to stay total.
//
// The arity for the binding is the max of the paren count and the
// braced-lvalue count: the paren count undercounts structs with reference
// members (plain any cannot initialize them), while the braced-lvalue
// count gets those right; for every non-array struct the max is the true
// count. (For C-array structs both counts are wrong and the binding fails
// loudly, which is the documented behavior there.)
template <typename T>
consteval std::size_t count_for_backstop() {
    const std::size_t paren = count_paren<T>();
    const std::size_t braced = count_braced<any_lvalue, T>();
    return paren > braced ? paren : braced;
}

template <typename T>
constexpr bool has_reference_field_decltype_v =
    decltype(has_reference_field_tag(
        std::declval<T&>(),
        std::integral_constant<std::size_t, count_for_backstop<T>()>{}))::value;

}  // namespace detail

// An aggregate the reflection engine can decompose: no base classes with
// data members alongside derived members (structured bindings cannot
// decompose those - a loud compile error if attempted), no unions, no
// arrays as the type itself, at most 64 fields, no (non-const) reference
// members. A struct with a C-style array member may still satisfy this
// concept while its count is wrong; using to_tuple/get/for_each_field on
// one fails at compile time with a structured-binding count mismatch.
// Const reference members satisfy this concept too; the serializer rejects
// them separately (see serializer.hxx).
template <typename T>
concept reflectable =
    std::is_aggregate_v<T> && !std::is_union_v<T> && !std::is_array_v<T> &&
    (detail::count_paren<T>() <= 64) &&
    (detail::count_paren<T>() > 0 || std::is_empty_v<T>) &&
    !detail::has_reference_field<T>();

// Number of fields of an aggregate. Returns 65 for aggregates with more
// than 64 fields (the probe range is 0..65, so 65 means "over the cap").
template <typename T>
consteval std::size_t field_count() {
    static_assert(std::is_aggregate_v<T>,
                  "simulnetx::reflect::field_count requires an aggregate type");
    return detail::count_paren<T>();
}

// The fields of obj as a tuple of references, in declaration order.
// For an rvalue the references are rvalue references; for a const object
// they are const.
template <typename T>
constexpr auto to_tuple(T&& obj) {
    static_assert(reflectable<std::remove_cvref_t<T>>,
                  "simulnetx::reflect::to_tuple requires a reflectable aggregate: "
                  "no base classes with members, no unions, no reference members, "
                  "at most 64 fields, and no C-style array members (use std::array)");
    constexpr std::size_t n = field_count<std::remove_cvref_t<T>>();
    return detail::to_tuple_impl(std::forward<T>(obj), std::integral_constant<std::size_t, n>{});
}

// The I-th field of obj, as a reference. An out-of-range I is a compile
// error from std::get.
template <std::size_t I, typename T>
constexpr decltype(auto) get(T&& obj) {
    return std::get<I>(to_tuple(std::forward<T>(obj)));
}

// Call f on each field of obj, in declaration order. f receives each field
// as a reference (const if obj is const).
template <typename T, typename F>
constexpr void for_each_field(T&& obj, F&& f) {
    std::apply(
        [&f](auto&&... fields) { ((void)f(fields), ...); },
        to_tuple(std::forward<T>(obj)));
}

}  // namespace simulnetx::reflect
