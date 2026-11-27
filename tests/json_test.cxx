// JSON value model, writer and encoder (see json.hxx). The golden
// trace_event test pins field order, enum names and number formatting;
// the dummy reflectable struct proves the field_names mechanism outside
// trace.

#include <array>
#include <charconv>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <limits>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <tuple>
#include <utility>
#include <variant>
#include <vector>

#include <gtest/gtest.h>

#include "simulnetx/json.hxx"
#include "simulnetx/network.hxx"
#include "simulnetx/scenario.hxx"
#include "simulnetx/trace.hxx"

namespace {

using namespace std::chrono_literals;
using simulnetx::drop_reason;
using simulnetx::event_kind;
using simulnetx::json::array;
using simulnetx::json::object;
using simulnetx::json::value;
using simulnetx::trace_event;

// A dummy reflectable struct, proving the field_names mechanism outside
// trace: the encoder walks its values, the table supplies the names.
struct JsonTestPoint {
    int x{};
    std::string label;
    std::optional<int> maybe{};
};

enum class JsonTestColor : int { red = 1, blue = 2 };
enum class JsonTestBig : unsigned long { huge = 4000000000UL };

TEST(JsonTest, DumpTraceJsonEmpty) {
    simulnetx::network net(42);
    net.add_node("A");
    EXPECT_EQ(net.dump_trace_json(), "{\"seed\":42,\"events\":[]}");
}

TEST(JsonTest, DumpTraceJsonGolden) {
    // The same tiny scenario as TraceDumpTest.GoldenTinyExample, in JSON:
    // A -> B, 50ms latency, no loss, one send at t=0. The exact string,
    // byte for byte, on both compilers.
    simulnetx::network net(42);
    auto& a = net.add_node("A");
    auto& b = net.add_node("B");
    net.link(a.id(), b.id(), {.latency = 50ms});
    a.send(b.id(), "hi");
    net.run_for(1s);

    EXPECT_EQ(net.dump_trace_json(),
              "{\"seed\":42,\"events\":["
              "{\"at\":0,\"kind\":\"sent\",\"from\":0,\"to\":1,\"bytes\":2,\"tag\":0,"
              "\"reason\":\"loss\",\"timer\":0,\"node\":0,\"request_id\":0,\"attempt\":0},"
              "{\"at\":50000000,\"kind\":\"delivered\",\"from\":0,\"to\":1,\"bytes\":2,"
              "\"tag\":0,\"reason\":\"loss\",\"timer\":0,\"node\":0,\"request_id\":0,"
              "\"attempt\":0}]}");
    std::ostringstream oss;
    net.dump_trace_json(oss);
    EXPECT_EQ(oss.str(), net.dump_trace_json());
}

// A small lossy scenario, run twice: the JSON must be identical.
std::string run_lossy_json() {
    simulnetx::network net(7);
    auto& a = net.add_node("A");
    auto& b = net.add_node("B");
    net.link(a.id(), b.id(), {.latency = 20ms, .loss_rate = 0.3});
    b.on_message([&](const simulnetx::message& m) {
        if (m.payload.size() == 4) {
            b.send(a.id(), "ack");
        }
    });
    std::function<void()> retry;
    int attempts = 0;
    retry = [&] {
        if (attempts >= 5) {
            return;
        }
        ++attempts;
        a.send(b.id(), "ping");
        a.after(100ms, retry);
    };
    a.after(0ns, retry);
    net.run_for(1s);
    return net.dump_trace_json();
}

TEST(JsonTest, DumpTraceJsonDeterminism) {
    EXPECT_EQ(run_lossy_json(), run_lossy_json());
}

TEST(JsonTest, DumpTraceJsonTransparency) {
    // Exporting changes no counts (the v0.5.0 pattern): tracing off gives
    // identical stats and an empty events array.
    simulnetx::network traced(7);
    {
        auto& a = traced.add_node("A");
        auto& b = traced.add_node("B");
        traced.link(a.id(), b.id(), {.latency = 20ms, .loss_rate = 0.3});
        a.send(b.id(), "ping");
        traced.run_for(1s);
    }
    simulnetx::network untraced(7);
    untraced.set_tracing(false);
    {
        auto& a = untraced.add_node("A");
        auto& b = untraced.add_node("B");
        untraced.link(a.id(), b.id(), {.latency = 20ms, .loss_rate = 0.3});
        a.send(b.id(), "ping");
        untraced.run_for(1s);
    }
    EXPECT_EQ(untraced.sent_count(), traced.sent_count());
    EXPECT_EQ(untraced.delivered_count(), traced.delivered_count());
    EXPECT_EQ(untraced.dropped_count(), traced.dropped_count());
    EXPECT_EQ(untraced.dump_trace_json(), "{\"seed\":7,\"events\":[]}");
    EXPECT_NE(traced.dump_trace_json(), "{\"seed\":7,\"events\":[]}");
}

TEST(JsonTest, DumpTraceJsonScenarioPassthrough) {
    simulnetx::scenario s("json-passthrough");
    s.seed(42).nodes("A", "B").link("A", "B", {.latency = 50ms});
    s.send_at(0ns, "A", "B", "hi");
    s.run_until(1s);
    // send_at fires through a network timer, so the trace starts with
    // its timer_fired event (node 4294967295 is no_node) before the send.
    EXPECT_EQ(s.dump_trace_json(),
              "{\"seed\":42,\"events\":["
              "{\"at\":0,\"kind\":\"timer_fired\",\"from\":0,\"to\":0,\"bytes\":0,"
              "\"tag\":0,\"reason\":\"loss\",\"timer\":0,\"node\":4294967295,"
              "\"request_id\":0,\"attempt\":0},"
              "{\"at\":0,\"kind\":\"sent\",\"from\":0,\"to\":1,\"bytes\":2,\"tag\":0,"
              "\"reason\":\"loss\",\"timer\":0,\"node\":0,\"request_id\":0,\"attempt\":0},"
              "{\"at\":50000000,\"kind\":\"delivered\",\"from\":0,\"to\":1,\"bytes\":2,"
              "\"tag\":0,\"reason\":\"loss\",\"timer\":0,\"node\":0,\"request_id\":0,"
              "\"attempt\":0}]}");
}

}  // namespace

namespace simulnetx::json {

template <>
struct field_names<JsonTestPoint> {
    static constexpr std::array<std::string_view, 3> value = {"x", "label", "maybe"};
};

}  // namespace simulnetx::json

namespace {

TEST(JsonTest, WriterEmptyObjectAndArray) {
    EXPECT_EQ(simulnetx::json::dump(value{object{}}), "{}");
    EXPECT_EQ(simulnetx::json::dump(value{array{}}), "[]");
    EXPECT_EQ(simulnetx::json::dump(value{nullptr}), "null");
    EXPECT_EQ(simulnetx::json::dump(value{true}), "true");
    EXPECT_EQ(simulnetx::json::dump(value{false}), "false");
}

TEST(JsonTest, WriterNestedObjectAndArray) {
    const value nested{object{
        {"a", value{array{value{std::int64_t{1}}, value{std::string{"x"}}}}},
        {"b", value{nullptr}},
        {"c", value{object{{"d", value{std::uint64_t{2}}}}}},
    }};
    EXPECT_EQ(simulnetx::json::dump(nested), "{\"a\":[1,\"x\"],\"b\":null,\"c\":{\"d\":2}}");
}

TEST(JsonTest, WriterEscaping) {
    // Quotes, backslash, the six short escapes, then two control chars
    // as \u00XX (lowercase hex).
    const std::string raw = "\"\\\b\f\n\r\t\x01\x1F";
    EXPECT_EQ(simulnetx::json::dump(value{raw}),
              "\"\\\"\\\\\\b\\f\\n\\r\\t\\u0001\\u001f\"");
    // Bytes >= 0x80 pass through as UTF-8, unescaped.
    EXPECT_EQ(simulnetx::json::dump(value{std::string{"caf\xC3\xA9"}}),
              "\"caf\xC3\xA9\"");
    // Keys are escaped the same way as string values.
    const value keyed{object{{"a\"b", value{std::int64_t{1}}}}};
    EXPECT_EQ(simulnetx::json::dump(keyed), "{\"a\\\"b\":1}");
}

TEST(JsonTest, WriterDoubles) {
    EXPECT_EQ(simulnetx::json::dump(value{0.1}), "0.1");
    EXPECT_EQ(simulnetx::json::dump(value{2.5}), "2.5");
    EXPECT_EQ(simulnetx::json::dump(value{-2.75}), "-2.75");
    EXPECT_EQ(simulnetx::json::dump(value{2.0}), "2");
    // Shortest round-trip: parse the output back, get the same double.
    for (const double d : {0.1, 1.5, -2.75, 1e20, 1e-7, 123456.789}) {
        const std::string text = simulnetx::json::dump(value{d});
        double parsed = 0.0;
        const auto result =
            std::from_chars(text.data(), text.data() + text.size(), parsed);
        ASSERT_EQ(result.ec, std::errc{});
        EXPECT_EQ(parsed, d) << "round-trip of " << d << " via " << text;
    }
}

TEST(JsonTest, WriterNonFiniteDoubleThrows) {
    EXPECT_THROW(simulnetx::json::dump(value{std::numeric_limits<double>::quiet_NaN()}),
                 std::invalid_argument);
    EXPECT_THROW(simulnetx::json::dump(value{std::numeric_limits<double>::infinity()}),
                 std::invalid_argument);
}

TEST(JsonTest, WriterStreamEqualsString) {
    const value nested{object{{"a", value{array{value{std::int64_t{1}}}}}}};
    std::ostringstream oss;
    simulnetx::json::dump(nested, oss);
    EXPECT_EQ(oss.str(), simulnetx::json::dump(nested));
}

TEST(JsonTest, EncoderPrimitives) {
    EXPECT_EQ(simulnetx::json::dump(simulnetx::json::to_json(true)), "true");
    EXPECT_EQ(simulnetx::json::dump(simulnetx::json::to_json(-5)), "-5");
    EXPECT_EQ(simulnetx::json::dump(simulnetx::json::to_json(5U)), "5");
    EXPECT_EQ(simulnetx::json::dump(simulnetx::json::to_json(2.5)), "2.5");
    EXPECT_EQ(simulnetx::json::dump(simulnetx::json::to_json(std::string{"hi"})), "\"hi\"");
    EXPECT_EQ(simulnetx::json::dump(simulnetx::json::to_json(std::string_view{"hi"})),
              "\"hi\"");
    // A char encodes as its integer value, not as a one-char string.
    EXPECT_EQ(simulnetx::json::dump(simulnetx::json::to_json('A')), "65");
}

TEST(JsonTest, EncoderDurationIsItsCount) {
    // Durations encode as .count() in their own tick unit: nanoseconds
    // stay nanoseconds, milliseconds stay milliseconds (no conversion).
    EXPECT_EQ(simulnetx::json::dump(simulnetx::json::to_json(42ns)), "42");
    EXPECT_EQ(simulnetx::json::dump(simulnetx::json::to_json(1500ms)), "1500");
}

TEST(JsonTest, EncoderEnums) {
    // A generic enum encodes as its integer value...
    EXPECT_EQ(simulnetx::json::dump(simulnetx::json::to_json(JsonTestColor::blue)), "2");
    EXPECT_EQ(simulnetx::json::dump(simulnetx::json::to_json(JsonTestBig::huge)),
              "4000000000");
    // ...while the trace enums get their text-format names.
    EXPECT_EQ(simulnetx::json::dump(simulnetx::json::to_json(event_kind::delivered)),
              "\"delivered\"");
    EXPECT_EQ(simulnetx::json::dump(simulnetx::json::to_json(drop_reason::crashed)),
              "\"crashed\"");
}

TEST(JsonTest, EncoderContainers) {
    EXPECT_EQ(simulnetx::json::dump(
                  simulnetx::json::to_json(std::vector<int>{1, 2, 3})),
              "[1,2,3]");
    EXPECT_EQ(simulnetx::json::dump(
                  simulnetx::json::to_json(std::array<int, 2>{4, 5})),
              "[4,5]");
    EXPECT_EQ(simulnetx::json::dump(simulnetx::json::to_json(
                  std::pair<int, std::string>{1, "x"})),
              "[1,\"x\"]");
    EXPECT_EQ(simulnetx::json::dump(simulnetx::json::to_json(
                  std::tuple<int, double, std::string>{1, 2.5, "s"})),
              "[1,2.5,\"s\"]");
    EXPECT_EQ(simulnetx::json::dump(simulnetx::json::to_json(std::optional<int>{})),
              "null");
    EXPECT_EQ(simulnetx::json::dump(simulnetx::json::to_json(std::optional<int>{7})),
              "7");
    const std::variant<int, std::string> as_string{std::string{"hi"}};
    EXPECT_EQ(simulnetx::json::dump(simulnetx::json::to_json(as_string)), "\"hi\"");
    const std::variant<int, std::string> as_int{3};
    EXPECT_EQ(simulnetx::json::dump(simulnetx::json::to_json(as_int)), "3");
}

TEST(JsonTest, EncoderReflectableDummyStruct) {
    // Values walk automatically; names come from the one table.
    const JsonTestPoint filled{3, "p", 9};
    EXPECT_EQ(simulnetx::json::dump(simulnetx::json::to_json(filled)),
              "{\"x\":3,\"label\":\"p\",\"maybe\":9}");
    const JsonTestPoint empty{0, "", std::nullopt};
    EXPECT_EQ(simulnetx::json::dump(simulnetx::json::to_json(empty)),
              "{\"x\":0,\"label\":\"\",\"maybe\":null}");
}

TEST(JsonTest, TraceEventHasElevenFields) {
    // The design doc said 13; the struct has 11 (at, kind, from, to,
    // bytes, tag, reason, timer, node, request_id, attempt). The
    // field_names table below must match this count.
    EXPECT_EQ(simulnetx::reflect::field_count<trace_event>(), 11U);
}

TEST(JsonTest, TraceEventGolden) {
    // One delivered event, every field pinned: order, enum names, number
    // formatting. Irrelevant fields carry their defaults (reason "loss",
    // node 0): the encoder emits all fields, uniformly.
    const trace_event delivered{.at = 20ms,
                                .kind = event_kind::delivered,
                                .from = 0,
                                .to = 1,
                                .bytes = 4};
    EXPECT_EQ(simulnetx::json::dump(simulnetx::json::to_json(delivered)),
              "{\"at\":20000000,\"kind\":\"delivered\",\"from\":0,\"to\":1,\"bytes\":4,"
              "\"tag\":0,\"reason\":\"loss\",\"timer\":0,\"node\":0,\"request_id\":0,"
              "\"attempt\":0}");
    const trace_event request{.kind = event_kind::request_sent,
                              .from = 1,
                              .to = 0,
                              .request_id = 3,
                              .attempt = 2};
    EXPECT_EQ(simulnetx::json::dump(simulnetx::json::to_json(request)),
              "{\"at\":0,\"kind\":\"request_sent\",\"from\":1,\"to\":0,\"bytes\":0,"
              "\"tag\":0,\"reason\":\"loss\",\"timer\":0,\"node\":0,\"request_id\":3,"
              "\"attempt\":2}");
    // A tag above 2^53 still prints exactly (JSON number, not a double).
    const trace_event tagged{.kind = event_kind::sent,
                             .from = 0,
                             .to = 1,
                             .bytes = 8,
                             .tag = 13776757609873999785ULL};
    EXPECT_EQ(simulnetx::json::dump(simulnetx::json::to_json(tagged)),
              "{\"at\":0,\"kind\":\"sent\",\"from\":0,\"to\":1,\"bytes\":8,"
              "\"tag\":13776757609873999785,\"reason\":\"loss\",\"timer\":0,\"node\":0,"
              "\"request_id\":0,\"attempt\":0}");
}

}  // namespace
