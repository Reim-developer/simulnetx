// The JSON writer: one non-template function walking the value model.
// Escaping lives in exactly one place; see json.hxx for the format rules.

#include "simulnetx/json.hxx"

#include <charconv>
#include <cmath>
#include <ostream>
#include <sstream>
#include <stdexcept>

namespace simulnetx::json {

namespace {

void write_escaped(std::ostream& out, std::string_view text) {
    out << '"';
    for (const unsigned char c : text) {
        switch (c) {
            case '"':
                out << "\\\"";
                break;
            case '\\':
                out << "\\\\";
                break;
            case '\b':
                out << "\\b";
                break;
            case '\f':
                out << "\\f";
                break;
            case '\n':
                out << "\\n";
                break;
            case '\r':
                out << "\\r";
                break;
            case '\t':
                out << "\\t";
                break;
            default:
                if (c < 0x20) {
                    // \u00XX, lowercase hex.
                    constexpr char hex[] = "0123456789abcdef";
                    out << "\\u00" << hex[(c >> 4) & 0xF] << hex[c & 0xF];
                } else {
                    // Bytes >= 0x80 pass through as UTF-8.
                    out << static_cast<char>(c);
                }
        }
    }
    out << '"';
}

void write_double(std::ostream& out, double d) {
    // NaN and infinities have no JSON representation: loud, never a
    // bare "nan" in the output.
    if (!std::isfinite(d)) {
        throw std::invalid_argument(
            "simulnetx: json cannot encode a non-finite double (NaN and "
            "infinities have no JSON representation)");
    }
    // std::to_chars: shortest round-trip, correctly rounded, and
    // therefore deterministic across compilers.
    char buf[32];
    const std::to_chars_result result = std::to_chars(buf, buf + sizeof(buf), d);
    out.write(buf, result.ptr - buf);
}

void write_value(std::ostream& out, const value& v) {
    std::visit(
        [&out](const auto& alternative) {
            using alternative_t = std::decay_t<decltype(alternative)>;
            if constexpr (std::is_same_v<alternative_t, std::nullptr_t>) {
                out << "null";
            } else if constexpr (std::is_same_v<alternative_t, bool>) {
                out << (alternative ? "true" : "false");
            } else if constexpr (std::is_same_v<alternative_t, std::int64_t> ||
                                 std::is_same_v<alternative_t, std::uint64_t>) {
                out << alternative;
            } else if constexpr (std::is_same_v<alternative_t, double>) {
                write_double(out, alternative);
            } else if constexpr (std::is_same_v<alternative_t, std::string>) {
                write_escaped(out, alternative);
            } else if constexpr (std::is_same_v<alternative_t, array>) {
                out << '[';
                bool first = true;
                for (const value& element : alternative) {
                    if (!first) {
                        out << ',';
                    }
                    first = false;
                    write_value(out, element);
                }
                out << ']';
            } else if constexpr (std::is_same_v<alternative_t, object>) {
                out << '{';
                bool first = true;
                for (const auto& [key, element] : alternative) {
                    if (!first) {
                        out << ',';
                    }
                    first = false;
                    write_escaped(out, key);
                    out << ':';
                    write_value(out, element);
                }
                out << '}';
            }
        },
        v);
}

}  // namespace

std::string dump(const value& v) {
    std::ostringstream oss;
    dump(v, oss);
    return oss.str();
}

void dump(const value& v, std::ostream& out) { write_value(out, v); }

}  // namespace simulnetx::json
