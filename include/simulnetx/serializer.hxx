#pragma once

// Sim-scoped serialization for typed messages.
//
// Scope, deliberately narrow: same process, same binary, test payloads.
// There is no schema evolution, no versioning, no endianness handling, no
// cross-language support - this is not a second cereal or protobuf, it is
// the minimum that lets send<T>/as<T> exist. Anything it cannot handle
// fails loudly at compile time (no serializer) or at decode time
// (decode_error), never silently.
//
// Layout: trivially-copyable types are memcpy'd; std::string and
// std::vector are length-prefixed (u64 count, then elements); aggregates
// serialize field by field via simulnetx::reflect, so a plain struct with
// std::string or std::vector members needs no boilerplate at all. A
// user-defined type opts in by specializing serializer<T>; the
// specialization wins over the reflection and memcpy paths.
//
// The type tag (see type_tag) is NOT written by the serializer: it is the
// wire envelope's job (node::send<T> writes it, message::as<T> checks it),
// because the request/response engine reuses the serializer for payloads
// inside its own envelopes.

#include <array>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <new>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <tuple>
#include <type_traits>
#include <typeinfo>
#include <utility>
#include <variant>
#include <vector>

#include "simulnetx/errors.hxx"
#include "simulnetx/reflect.hxx"

namespace simulnetx {

namespace detail {

// FNV-1a 64-bit hash. The constants are the standard offset basis and
// prime; the hash itself is only used for type tags, never for simulation
// outcomes (the RNG is splitmix64, see rng.hxx).
constexpr std::uint64_t fnv1a_64(std::string_view text) noexcept {
    std::uint64_t hash = 14695981039346656037ULL;
    for (const char c : text) {
        hash ^= static_cast<unsigned char>(c);
        hash *= 1099511628211ULL;
    }
    return hash;
}

// The type tag: a 64-bit FNV-1a hash of the type's mangled name. Stable
// within one build; NOT stable across compilers, because typeid names
// differ - golden tests assert round-trip equality, never raw bytes.
// Cached once per type; two distinct types collide with probability 2^-64.
template <typename T>
std::uint64_t type_tag() {
    static const std::uint64_t tag =
        fnv1a_64(typeid(std::remove_cvref_t<T>).name());
    return tag;
}

// Envelope kind tags for the request/response engine: FNV-1a of string
// literals, so they are stable across compilers (unlike typeid-based
// tags). A request envelope is [request_envelope_tag][request_id]
// [req_tag][payload]; a response is [response_envelope_tag][request_id]
// [resp_tag][payload]. The kind tag is what lets a node tell a request
// from a response: without it, a request whose id collides with a pending
// id on the receiver would be misrouted as a response (request ids are
// per-node, so every node's first request is id 0), and a self-request
// would always misroute.
constexpr std::uint64_t request_envelope_tag = fnv1a_64("simulnetx::request");
constexpr std::uint64_t response_envelope_tag = fnv1a_64("simulnetx::response");

// A UDP datagram is [datagram_envelope_tag][src_port][dst_port][payload];
// see udp.hxx. Ports travel as u64 fields for uniform cursor parsing.
constexpr std::uint64_t datagram_envelope_tag = fnv1a_64("simulnetx::datagram");

// Appending helpers for the encode side. All appends are to the caller's
// vector; nothing here allocates on its own.
inline void append_bytes(std::vector<std::byte>& out, const std::byte* data,
                         std::size_t size) {
    out.insert(out.end(), data, data + size);
}

template <typename T>
void append_trivial(std::vector<std::byte>& out, const T& value) {
    static_assert(std::is_trivially_copyable_v<T>);
    const auto* bytes = reinterpret_cast<const std::byte*>(&value);
    append_bytes(out, bytes, sizeof(T));
}

inline void append_u64(std::vector<std::byte>& out, std::uint64_t value) {
    append_trivial(out, value);
}

// Read 8 bytes as a u64 in native byte order. Precondition: at least 8
// bytes are readable at data. Used to peek at a wire tag after the caller
// has checked the payload size; the checked path is cursor::take_u64.
inline std::uint64_t read_u64(const std::byte* data) {
    std::uint64_t value = 0;
    std::memcpy(&value, data, sizeof(value));
    return value;
}

// Reading position in a payload being decoded. Every read checks bounds
// and throws decode_error on overrun: a truncated payload is loud, never
// a buffer over-read.
class cursor {
public:
    cursor(const std::byte* data, std::size_t size) : ptr_(data), end_(data + size) {}

    [[nodiscard]] std::size_t remaining() const {
        return static_cast<std::size_t>(end_ - ptr_);
    }

    // Take n bytes, advancing the cursor. Throws decode_error if fewer
    // than n bytes remain.
    std::span<const std::byte> take(std::size_t size) {
        if (remaining() < size) {
            throw decode_error("simulnetx: decode_error: truncated payload (needed " +
                               std::to_string(size) + " bytes, had " +
                               std::to_string(remaining()) + ")");
        }
        const std::span<const std::byte> bytes(ptr_, size);
        ptr_ += size;
        return bytes;
    }

    template <typename T>
    T take_trivial() {
        static_assert(std::is_trivially_copyable_v<T>);
        const std::span<const std::byte> bytes = take(sizeof(T));
        // No default construction: T may not have a default constructor
        // (a user-provided constructor does not break trivial copyability).
        // memcpy into suitably aligned storage implicitly creates the
        // object (trivially-copyable types are implicit-lifetime types);
        // launder gives a usable pointer to it.
        alignas(T) std::byte storage[sizeof(T)];
        std::memcpy(storage, bytes.data(), sizeof(T));
        return *std::launder(reinterpret_cast<T*>(storage));
    }

    std::uint64_t take_u64() { return take_trivial<std::uint64_t>(); }

private:
    const std::byte* ptr_;
    const std::byte* end_;
};

// Which built-in containers have their own serializer partial below. The
// generic partial excludes them: partial ordering cannot rank an
// enable_if-based partial against a pattern-based one, so the generic
// partial must not match them at all (an enable_if condition that
// includes them is ambiguous, verified on g++ and clang++).
template <typename T>
struct is_builtin_container : std::false_type {};
template <>
struct is_builtin_container<std::string> : std::true_type {};
template <typename T>
struct is_builtin_container<std::vector<T>> : std::true_type {};
template <typename T>
struct is_builtin_container<std::optional<T>> : std::true_type {};
template <typename... Ts>
struct is_builtin_container<std::variant<Ts...>> : std::true_type {};
template <typename A, typename B>
struct is_builtin_container<std::pair<A, B>> : std::true_type {};
template <typename... Ts>
struct is_builtin_container<std::tuple<Ts...>> : std::true_type {};
template <typename T, std::size_t N>
struct is_builtin_container<std::array<T, N>> : std::true_type {};

template <typename T>
constexpr bool is_builtin_container_v = is_builtin_container<T>::value;

enum class serializer_kind { none, reflect, memcpy };

// How the generic partial handles T: reflection for aggregates (even when
// they are not trivially copyable - a struct with a std::string member),
// memcpy for everything else trivially copyable. Pointers and member
// pointers are never serialized: an address is meaningless in a decoded
// value, so they are excluded even though they are trivially copyable.
// Arrays are excluded too: they cannot be returned by value, so the
// generic partial's decode would be ill-formed - and the exclusion must
// live here, because node::send<T> is considered for string literals
// (T = char[N]) during overload resolution, where a hard error inside the
// serializable concept would break the raw string_view overload.
template <typename T>
consteval serializer_kind kind_of() {
    if constexpr (is_builtin_container_v<T>) {
        return serializer_kind::none;
    } else if constexpr (reflect::reflectable<T>) {
        return serializer_kind::reflect;
    } else if constexpr (std::is_trivially_copyable_v<T> && !std::is_pointer_v<T> &&
                         !std::is_member_pointer_v<T> && !std::is_array_v<T>) {
        return serializer_kind::memcpy;
    } else {
        return serializer_kind::none;
    }
}

}  // namespace detail

// The serializer trait. The primary is deliberately undefined: a type
// with no serializer is a compile error at the send/as site, and the
// serializable concept detects it. Specialize for your own type to opt
// in:
//
//   template <> struct simulnetx::serializer<money> {
//       static void encode(const money& value, std::vector<std::byte>& out);
//       static money decode(detail::cursor& in);
//   };
//
// encode appends to out; decode reads from in (and may throw decode_error
// on malformed data). A user specialization wins over the reflection and
// memcpy paths.
template <typename T, typename = void>
struct serializer;

// Generic partial: aggregates via reflection, everything else
// trivially-copyable via memcpy. Built-in containers are excluded here
// (they have their own partials below) so the overload set is unambiguous.
template <typename T>
struct serializer<T, std::enable_if_t<detail::kind_of<T>() != detail::serializer_kind::none>> {
    static void encode(const T& value, std::vector<std::byte>& out) {
        if constexpr (detail::kind_of<T>() == detail::serializer_kind::reflect) {
            // Reference members cannot be decoded: a decoded value has no
            // referent to rebind. Non-const references are excluded by
            // the reflectable concept already; this catches const
            // references, which the concept cannot see.
            static_assert(!reflect::detail::has_reference_field_decltype_v<T>,
                          "simulnetx: cannot serialize an aggregate with reference members "
                          "(including const references): a decoded value cannot rebind them. "
                          "Store values, or write a serializer<T> specialization.");
            reflect::for_each_field(value, [&out](const auto& field) {
                serializer<std::decay_t<decltype(field)>>::encode(field, out);
            });
        } else {
            detail::append_trivial(out, value);
        }
    }

    static T decode(detail::cursor& in) {
        if constexpr (detail::kind_of<T>() == detail::serializer_kind::reflect) {
            static_assert(!reflect::detail::has_reference_field_decltype_v<T>,
                          "simulnetx: cannot serialize an aggregate with reference members "
                          "(including const references): a decoded value cannot rebind them. "
                          "Store values, or write a serializer<T> specialization.");
            return std::make_from_tuple<T>(
                decode_fields(in, std::make_index_sequence<reflect::field_count<T>()>{}));
        } else {
            return in.take_trivial<T>();
        }
    }

private:
    // Decode each field in declaration order into a tuple, then build T
    // from it. The braced-init-list evaluation is sequenced left to right,
    // so the decode order matches the encode order. make_from_tuple uses
    // parenthesized aggregate initialization, so fields need no default
    // constructor.
    template <std::size_t... Is>
    static auto decode_fields(detail::cursor& in, std::index_sequence<Is...>) {
        using tuple_t = decltype(reflect::to_tuple(std::declval<T&>()));
        return std::tuple<std::decay_t<std::tuple_element_t<Is, tuple_t>>...>{
            serializer<std::decay_t<std::tuple_element_t<Is, tuple_t>>>::decode(in)...};
    }
};

// std::string: u64 byte length, then the raw bytes (no NUL terminator).
template <>
struct serializer<std::string> {
    static void encode(const std::string& value, std::vector<std::byte>& out) {
        detail::append_u64(out, static_cast<std::uint64_t>(value.size()));
        detail::append_bytes(out, reinterpret_cast<const std::byte*>(value.data()),
                             value.size());
    }

    static std::string decode(detail::cursor& in) {
        const std::uint64_t size = in.take_u64();
        const std::span<const std::byte> bytes = in.take(static_cast<std::size_t>(size));
        return std::string(reinterpret_cast<const char*>(bytes.data()), bytes.size());
    }
};

// std::vector<T>: u64 element count, then each element in order. No
// reserve: a corrupt count must fail in decode with decode_error, not in
// reserve with bad_alloc. (A forged huge count with a zero-byte element
// type would loop; payloads here are produced by this encoder from real
// vectors, so a count is always the true size.)
template <typename T>
struct serializer<std::vector<T>> {
    static void encode(const std::vector<T>& value, std::vector<std::byte>& out) {
        detail::append_u64(out, static_cast<std::uint64_t>(value.size()));
        for (const T& element : value) {
            serializer<T>::encode(element, out);
        }
    }

    static std::vector<T> decode(detail::cursor& in) {
        const std::uint64_t size = in.take_u64();
        std::vector<T> result;
        for (std::uint64_t i = 0; i < size; ++i) {
            result.push_back(serializer<T>::decode(in));
        }
        return result;
    }
};

// std::optional<T>: one flag byte (0 = empty, 1 = present), then the value
// if present. Any other flag byte is a decode_error, not a guess.
template <typename T>
struct serializer<std::optional<T>> {
    static void encode(const std::optional<T>& value, std::vector<std::byte>& out) {
        out.push_back(value.has_value() ? std::byte{1} : std::byte{0});
        if (value.has_value()) {
            serializer<T>::encode(*value, out);
        }
    }

    static std::optional<T> decode(detail::cursor& in) {
        const std::span<const std::byte> flag = in.take(1);
        if (flag[0] == std::byte{0}) {
            return std::nullopt;
        }
        if (flag[0] != std::byte{1}) {
            throw decode_error("simulnetx: decode_error: invalid optional flag "
                               "(expected 0 or 1)");
        }
        return serializer<T>::decode(in);
    }
};

// std::variant<Ts...>: u64 alternative index, then the active alternative.
// An out-of-range index is a decode_error. Dispatch goes through a table
// of per-alternative decoders so no alternative needs to be
// default-constructible.
template <typename... Ts>
struct serializer<std::variant<Ts...>> {
    static void encode(const std::variant<Ts...>& value, std::vector<std::byte>& out) {
        detail::append_u64(out, static_cast<std::uint64_t>(value.index()));
        std::visit(
            [&out](const auto& alternative) {
                serializer<std::decay_t<decltype(alternative)>>::encode(alternative, out);
            },
            value);
    }

    static std::variant<Ts...> decode(detail::cursor& in) {
        const std::uint64_t index = in.take_u64();
        if (index >= sizeof...(Ts)) {
            throw decode_error("simulnetx: decode_error: variant index out of range");
        }
        return decode_alternative(in, index, std::index_sequence_for<Ts...>{});
    }

private:
    template <std::size_t I>
    static std::variant<Ts...> decode_one(detail::cursor& in) {
        using alternative = std::variant_alternative_t<I, std::variant<Ts...>>;
        return std::variant<Ts...>(std::in_place_index<I>,
                                   serializer<alternative>::decode(in));
    }

    template <std::size_t... Is>
    static std::variant<Ts...> decode_alternative(detail::cursor& in, std::uint64_t index,
                                                  std::index_sequence<Is...>) {
        using decode_fn = std::variant<Ts...> (*)(detail::cursor&);
        constexpr decode_fn table[] = {&decode_one<Is>...};
        return table[index](in);
    }
};

// std::pair<A, B>: first, then second, in that order.
template <typename A, typename B>
struct serializer<std::pair<A, B>> {
    static void encode(const std::pair<A, B>& value, std::vector<std::byte>& out) {
        serializer<A>::encode(value.first, out);
        serializer<B>::encode(value.second, out);
    }

    static std::pair<A, B> decode(detail::cursor& in) {
        A first = serializer<A>::decode(in);
        B second = serializer<B>::decode(in);
        return {std::move(first), std::move(second)};
    }
};

// std::tuple<Ts...>: each element in order.
template <typename... Ts>
struct serializer<std::tuple<Ts...>> {
    static void encode(const std::tuple<Ts...>& value, std::vector<std::byte>& out) {
        std::apply(
            [&out](const auto&... elements) {
                (serializer<std::decay_t<decltype(elements)>>::encode(elements, out), ...);
            },
            value);
    }

    static std::tuple<Ts...> decode(detail::cursor& in) {
        return decode_elements(in, std::index_sequence_for<Ts...>{});
    }

private:
    template <std::size_t... Is>
    static std::tuple<Ts...> decode_elements(detail::cursor& in, std::index_sequence<Is...>) {
        return std::tuple<Ts...>{serializer<Ts>::decode(in)...};
    }
};

// std::array<T, N>: each element in order, no length (the length is the
// type). Decoded through a braced-init-list so T needs no default
// constructor.
template <typename T, std::size_t N>
struct serializer<std::array<T, N>> {
    static void encode(const std::array<T, N>& value, std::vector<std::byte>& out) {
        for (const T& element : value) {
            serializer<T>::encode(element, out);
        }
    }

    static std::array<T, N> decode(detail::cursor& in) {
        return decode_elements(in, std::make_index_sequence<N>{});
    }

private:
    template <std::size_t... Is>
    static std::array<T, N> decode_elements(detail::cursor& in, std::index_sequence<Is...>) {
        return std::array<T, N>{(static_cast<void>(Is), serializer<T>::decode(in))...};
    }
};

// True when T has a serializer: a user specialization, a built-in
// container, a reflectable aggregate, or a trivially-copyable value type.
// The sizeof check is load-bearing: naming the undefined primary in a
// plain requires-expression would pass (an incomplete type name is
// valid), but sizeof on it is ill-formed, so the concept is false.
//
// Deliberately non-recursive: a reflectable struct whose field has no
// serializer still satisfies this concept, and the encode/decode call
// then fails loudly at the use site. A recursive concept would have to
// compute field types, which hard-errors for C-array members inside a
// concept (the count is wrong there); failing at the use site keeps the
// concept total and the error at the line that asked for the impossible.
template <typename T>
concept serializable = requires {
    requires sizeof(serializer<std::remove_cvref_t<T>>) > 0;
};

}  // namespace simulnetx
