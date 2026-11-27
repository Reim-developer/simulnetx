#pragma once

#include <concepts>
#include <cstddef>
#include <cstdlib>
#include <tuple>
#include <type_traits>
#include <utility>

namespace simulnetx::reflect::detail {

// Forward declaration: the generated header's count_arity_impl overloads
// constrain on this probe, and the generated header is included before
// the probe is defined below.
template <typename T, std::size_t... Is>
constexpr bool paren_constructible(std::index_sequence<Is...>);

}  // namespace simulnetx::reflect::detail

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

// Probe type. any_value converts to any value type (as a prvalue); it
// cannot initialize a non-const reference member, because a prvalue does
// not bind to a non-const lvalue reference - and a reference member cannot
// be value-initialized either. That is what the braced reference check
// below relies on (see braced_constructible_at).
//
// The conversion operator is DEFINED, not just declared: clang
// instantiates std::optional's converting constructor while evaluating
// the braced check for a struct with an optional member, and that
// constructor odr-uses this conversion, so a declaration-only operator
// fails the TU with -Wundefined-inline under -Werror (found by the v0.8.0
// JSON spike: a reflectable struct with an std::optional<int> member
// through for_each_field). The body is std::abort(): it is never
// executed - the probes only ever use the conversion in unevaluated
// contexts (is_constructible, requires-expressions) - and abort is the
// honest body for a function that must exist but must never run: it needs
// no construction of T, so it works for types with no default constructor
// (a `return T{};` body breaks structs with no_default members) and for
// move-only members alike. Not constexpr: a body that always aborts can
// never be a constant expression, and a constexpr function that can never
// be constant-evaluated is ill-formed (no diagnostic required).
struct any_value {
    template <typename T>
    operator T() const noexcept {
        std::abort();
    }
};

// Field count probe: direct (paren) initialization, which - unlike braced
// init - does not apply brace elision, so nested aggregates count as one
// field each. std::is_constructible uses direct init. The count is the
// largest N in [0, 201] for which T is constructible from N probe arguments;
// 201 means "more than 200 fields" and is never a valid count.
//
// Caveat: N == 1 is polluted by the copy/move constructor (any_value
// converts to T itself), so a 1-count on an empty type really means 0
// fields. That is corrected below.
template <typename T, std::size_t... Is>
constexpr bool paren_constructible(std::index_sequence<Is...>) {
    return std::is_constructible_v<T, decltype((static_cast<void>(Is), any_value{}))...>;
}

// Field count, via overload resolution over the generated per-arity
// probes (count_arity_impl in reflect_generated.hxx): each overload is
// viable iff T is constructible from that many probes, and the
// priority_tag ranking picks the highest viable arity — which is the
// field count, because the probe predicate is true exactly on the
// interval [j, k] (j = first index whose suffix is all
// default-constructible, k = field count), so its maximum is k.
//
// Why overload resolution, not a scan: the count is the maximum of a
// 202-wide probe, and MSVC errors C1202 ("recursive type or function
// dependency context too complex") when all 202 probes are evaluated
// inside one consteval function — proven three ways (CI runs #22–#24):
// a flat 202-wide fold, 32-wide chunks called directly, and 32-wide
// chunks behind variable templates all failed inside the first chunk
// for a type with non-default-constructible members
// (with_nodefault_members), while a 200-field all-int type passed the
// same scan. Overload resolution evaluates each candidate's constraint
// in its own SFINAE context, so no single evaluation holds more than
// one probe. (Binary search is unsound here: the predicate is false
// below j, so a search can miss the interval entirely.)
template <typename T>
consteval std::size_t count_paren() {
    // priority_tag<201> must match the generator's cap + 1 (see
    // tools/generate_reflect.py): 201 is the "over the cap" sentinel.
    constexpr std::size_t found =
        decltype(count_arity_impl<T>(priority_tag<201>{}))::value;
    if (found == 1 && std::is_empty_v<T>) {
        return 0;
    }
    return found;
}

// Memoized per-type count: the concept, field_count,
// has_reference_field and the serializer backstop all need it, and each
// evaluation is an overload resolution over 202 candidates. One
// evaluation per type per translation unit.
template <typename T>
inline constexpr std::size_t count_paren_v = count_paren<T>();

// Single-arity braced check, for reference detection only. The arity is
// the paren count: for a struct without reference members, braced init
// from that many prvalue probes succeeds; for a struct with a (non-const)
// reference member it fails, because a prvalue cannot initialize the
// reference. Braced init has no copy-ctor pollution - unlike paren init,
// where N == 1 succeeds through the copy constructor for any copyable
// struct - so the braced check sees what the paren count hides.
//
// This is ONE requires-expression, not a 66-wide scan: an earlier version
// scanned all arities with braced init and MSVC rejected it with C1202
// (recursive dependency context too complex). And there is deliberately
// no lvalue probe: clang 18 ICEs on the paren form with an lvalue probe
// (crash in CXXParenListInitExpr::computeDependence for a struct with a
// move-only member).
template <typename T, std::size_t... Is>
consteval bool braced_constructible_at(std::index_sequence<Is...>) {
    return requires { T{(static_cast<void>(Is), any_value{})...}; };
}

// True iff T has a non-const reference member. A prvalue cannot initialize
// one, so braced init from the paren count's worth of prvalue probes fails
// exactly when such a member is present. Const reference members are NOT
// detected here (a prvalue binds to a const reference); they are caught by
// has_reference_field_decltype_v, the serializer backstop.
template <typename T>
consteval bool has_reference_field() {
    return !braced_constructible_at<T>(std::make_index_sequence<count_paren_v<T>>{});
}

// Decltype-based reference detection (generated per-arity machinery): the
// binding of a reference member has reference type, a value member's does
// not. This catches const references too, but it instantiates a structured
// binding with the probed count, so it is a HARD ERROR for types whose
// count is wrong (C-array members) - it must only be used at use sites
// (the serializer's static_assert), never inside the reflectable concept,
// which has to stay total.
//
// The arity is the paren count: for const-reference members - the case
// this backstop exists for - the prvalue probe binds, so the count is the
// true field count. For non-const references the count is 0, but those
// structs never reach the serializer: the concept excludes them (a
// non-const reference member makes the count 0, and the concept requires
// a positive count for non-empty types).
template <typename T>
constexpr bool has_reference_field_decltype_v =
    decltype(has_reference_field_tag(
        std::declval<T&>(),
        std::integral_constant<std::size_t, count_paren_v<T>>{}))::value;

}  // namespace detail

// An aggregate the reflection engine can decompose: no base classes with
// data members alongside derived members (structured bindings cannot
// decompose those - a loud compile error if attempted), no unions, no
// arrays as the type itself, at most 200 fields, no (non-const) reference
// members. A struct with a C-style array member may still satisfy this
// concept while its count is wrong; using to_tuple/get/for_each_field on
// one fails at compile time with a structured-binding count mismatch.
// Const reference members satisfy this concept too; the serializer rejects
// them separately (see serializer.hxx).
//
// Reference members are rejected through the braced check
// (has_reference_field): a prvalue cannot initialize a non-const reference,
// so braced init from the paren count's worth of prvalue probes fails
// exactly when such a member is present. One hole: a reference member WITH
// a default member initializer, not in first position, passes the braced
// check (the members before it initialize from probes, the reference takes
// its default) and satisfies the concept - using to_tuple or the serializer
// on such a struct then fails at compile time with a structured-binding
// count mismatch, because the probed count is the reference's position, not
// the field count. No value-based probe can close this hole (see the note
// on braced_constructible_at above); a struct with a defaulted reference
// member is not a sensible message type.
template <typename T>
concept reflectable =
    std::is_aggregate_v<T> && !std::is_union_v<T> && !std::is_array_v<T> &&
    (detail::count_paren_v<T> <= 200) &&
    (detail::count_paren_v<T> > 0 || std::is_empty_v<T>) &&
    !detail::has_reference_field<T>();

// Number of fields of an aggregate. Returns 201 for aggregates with more
// than 200 fields (the probe range is 0..201, so 201 means "over the cap").
template <typename T>
consteval std::size_t field_count() {
    static_assert(std::is_aggregate_v<T>,
                  "simulnetx::reflect::field_count requires an aggregate type");
    return detail::count_paren_v<T>;
}

// The fields of obj as a tuple of references, in declaration order.
// For an rvalue the references are rvalue references; for a const object
// they are const.
template <typename T>
constexpr auto to_tuple(T&& obj) {
    static_assert(reflectable<std::remove_cvref_t<T>>,
                  "simulnetx::reflect::to_tuple requires a reflectable aggregate: "
                  "no base classes with members, no unions, no reference members, "
                  "at most 200 fields, and no C-style array members (use std::array)");
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
