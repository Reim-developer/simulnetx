#pragma once

// JSON trace export (v0.8.0 design): a tiny sim-scoped JSON library.
//
// Two parts, with the value model as the seam between them:
//  - the ENCODER (template): to_json<T> walks values via the v0.4.0
//    reflect machinery and builds json::value; one non-template WRITER
//    (see json.cxx) prints a value. The writer is not templated per
//    struct: a writer per type would be binary bloat, untestable in
//    isolation. Escaping, commas, nesting and type coverage live in one
//    tested place.
//
// Two things C++20 cannot provide, so they are explicit tables, not
// templates:
//  - Field NAMES: structured bindings give values, not names (the same
//    reason field_name was deferred in v0.4.0). A reflectable type needs
//    a field_names<T> specialization listing its names in declaration
//    order; using to_json on one without it is a static_assert, not a
//    silent wrong answer. Adding a field still shows up automatically in
//    the values; the name list is one line per field, in one place.
//  - Enum NAMES: reflect cannot enumerate enumerators either. event_kind
//    and drop_reason get explicit to_json overloads returning the
//    text-format names from trace.hxx; any other enum encodes as its
//    integer value.
//
// The public trace API is only network::dump_trace_json (see
// network.hxx); this header is the machinery behind it. Compact output,
// no pretty-printing: deterministic, byte-identical across compilers.

#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <ostream>
#include <string>
#include <string_view>
#include <tuple>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

#include "simulnetx/reflect.hxx"
#include "simulnetx/trace.hxx"

namespace simulnetx::json {

// A JSON array, in order. A JSON object, as insertion-ordered pairs, NOT
// a map: field order is declaration order (readable, and it matches the
// text dump), and determinism comes from the encoder inserting in
// declaration order. Lookup is linear; these objects are small.
struct value;
using array = std::vector<value>;
using object = std::vector<std::pair<std::string, value>>;

// json::value: null / bool / int64 / uint64 / double / string / array /
// object. A struct inheriting the variant rather than an alias: the
// variant is recursive (array/object hold values), and a plain alias
// cannot name itself; the vector breaks the recursion, so this is the
// standard recursive-variant shape. Construction is the variant's
// (value{std::int64_t{42}}, value{std::string{"x"}}); spell the type
// out - a bare 42 or "text" would let the variant's converting
// constructor guess the alternative.
struct value : std::variant<std::nullptr_t, bool, std::int64_t, std::uint64_t, double,
                            std::string, array, object> {
    using variant::variant;
};

// Field names for a reflectable type, in declaration order. The primary
// is deliberately undefined: to_json on a reflectable type without a
// specialization is a static_assert (see below), not a silent wrong
// answer. In v0.8.0 the only specialization is trace_event.
template <typename T>
struct field_names;

// True when T has a field_names specialization. The sizeof check is
// load-bearing (same trick as the serializable concept): naming the
// undefined primary in a plain requires-expression would pass, but
// sizeof on it is ill-formed, so the concept is false.
template <typename T>
concept has_field_names = requires {
    requires sizeof(field_names<std::remove_cvref_t<T>>) > 0;
};

namespace detail {

template <typename T>
struct always_false : std::false_type {};

template <typename T>
struct is_duration : std::false_type {};
template <typename Rep, typename Period>
struct is_duration<std::chrono::duration<Rep, Period>> : std::true_type {};
template <typename T>
constexpr bool is_duration_v = is_duration<T>::value;

template <typename T>
struct is_optional : std::false_type {};
template <typename T>
struct is_optional<std::optional<T>> : std::true_type {};
template <typename T>
constexpr bool is_optional_v = is_optional<T>::value;

template <typename T>
struct is_vector : std::false_type {};
template <typename T, typename Alloc>
struct is_vector<std::vector<T, Alloc>> : std::true_type {};
template <typename T>
constexpr bool is_vector_v = is_vector<T>::value;

template <typename T>
struct is_array : std::false_type {};
template <typename T, std::size_t N>
struct is_array<std::array<T, N>> : std::true_type {};
template <typename T>
constexpr bool is_array_v = is_array<T>::value;

template <typename T>
struct is_pair : std::false_type {};
template <typename A, typename B>
struct is_pair<std::pair<A, B>> : std::true_type {};
template <typename T>
constexpr bool is_pair_v = is_pair<T>::value;

template <typename T>
struct is_tuple : std::false_type {};
template <typename... Ts>
struct is_tuple<std::tuple<Ts...>> : std::true_type {};
template <typename T>
constexpr bool is_tuple_v = is_tuple<T>::value;

template <typename T>
struct is_variant : std::false_type {};
template <typename... Ts>
struct is_variant<std::variant<Ts...>> : std::true_type {};
template <typename T>
constexpr bool is_variant_v = is_variant<T>::value;

}  // namespace detail

// Enum names: the text-format names from trace.hxx. Declared before the
// template so overload resolution inside to_json picks them for these
// types (a non-template overload wins the tie against the template).
inline value to_json(event_kind kind) {
    return value{std::string{event_kind_name(kind)}};
}
inline value to_json(drop_reason reason) {
    return value{std::string{drop_reason_name(reason)}};
}

// Encode a value as JSON. The template walks VALUES; names come from
// field_names<T> (reflectable aggregates) or from the structure itself
// (pairs/tuples become arrays).
//
//  - arithmetic: bool -> bool; signed -> int64; unsigned -> uint64;
//    floating -> double. A char encodes as its integer value (it is an
//    arithmetic type); there is no char-to-string special case.
//  - std::string / std::string_view -> string.
//  - std::chrono::duration -> its .count() as int64, in its own tick
//    unit (trace uses nanoseconds; the unit is the type's, not converted).
//  - std::optional<T> -> null or to_json(*v); std::vector<T> /
//    std::array<T, N> -> array; std::pair / std::tuple -> array;
//    std::variant -> the active alternative.
//  - reflectable aggregates -> object: for_each_field walks values,
//    field_names<T> supplies keys in the same order.
//  - enums: the integer value, except event_kind / drop_reason above.
template <typename T>
value to_json(const T& v) {
    using U = std::decay_t<T>;
    if constexpr (std::is_same_v<U, bool>) {
        return value{v};
    } else if constexpr (std::is_same_v<U, std::string> ||
                         std::is_same_v<U, std::string_view>) {
        return value{std::string{v}};
    } else if constexpr (detail::is_duration_v<U>) {
        return value{static_cast<std::int64_t>(v.count())};
    } else if constexpr (std::is_enum_v<U>) {
        using underlying = std::underlying_type_t<U>;
        if constexpr (std::is_signed_v<underlying>) {
            return value{static_cast<std::int64_t>(v)};
        } else {
            return value{static_cast<std::uint64_t>(v)};
        }
    } else if constexpr (std::is_arithmetic_v<U>) {
        if constexpr (std::is_floating_point_v<U>) {
            return value{static_cast<double>(v)};
        } else if constexpr (std::is_signed_v<U>) {
            return value{static_cast<std::int64_t>(v)};
        } else {
            return value{static_cast<std::uint64_t>(v)};
        }
    } else if constexpr (detail::is_optional_v<U>) {
        if (!v.has_value()) {
            return value{nullptr};
        }
        return to_json(*v);
    } else if constexpr (detail::is_vector_v<U> || detail::is_array_v<U>) {
        array elements;
        elements.reserve(v.size());
        for (const auto& element : v) {
            elements.push_back(to_json(element));
        }
        return value{std::move(elements)};
    } else if constexpr (detail::is_pair_v<U>) {
        return value{array{to_json(v.first), to_json(v.second)}};
    } else if constexpr (detail::is_tuple_v<U>) {
        array elements;
        std::apply(
            [&elements](const auto&... items) {
                (elements.push_back(to_json(items)), ...);
            },
            v);
        return value{std::move(elements)};
    } else if constexpr (detail::is_variant_v<U>) {
        return std::visit([](const auto& alternative) { return to_json(alternative); }, v);
    } else if constexpr (reflect::reflectable<U>) {
        static_assert(has_field_names<U>,
                      "simulnetx: to_json needs a field_names<T> specialization listing "
                      "the field names in declaration order (C++20 cannot reflect "
                      "names). Add one line per field, in declaration order.");
        constexpr auto names = field_names<U>::value;
        static_assert(names.size() == reflect::field_count<U>(),
                      "simulnetx: field_names<T> must list exactly one name per field, "
                      "in declaration order");
        object fields;
        fields.reserve(names.size());
        std::size_t i = 0;
        reflect::for_each_field(v, [&](const auto& field) {
            fields.emplace_back(std::string{names[i]}, to_json(field));
            ++i;
        });
        return value{std::move(fields)};
    } else {
        static_assert(detail::always_false<U>::value,
                      "simulnetx: to_json has no encoding for this type");
    }
}

// Field names for trace_event, in declaration order (see trace.hxx).
// trace_event has 11 fields; the count is checked by to_json's
// static_assert above.
template <>
struct field_names<trace_event> {
    static constexpr std::array<std::string_view, 11> value = {
        "at", "kind", "from", "to", "bytes", "tag",
        "reason", "timer", "node", "request_id", "attempt"};
};

// The writer: one non-template function, in json.cxx. Compact output, no
// pretty-printing. Escaping in exactly one place: " \ \b \f \n \r \t,
// other control chars as \u00XX; bytes >= 0x80 pass through as UTF-8.
// Doubles via std::to_chars (shortest round-trip, deterministic across
// compilers). Integers print as decimal.
std::string dump(const value& v);
void dump(const value& v, std::ostream& out);

}  // namespace simulnetx::json
