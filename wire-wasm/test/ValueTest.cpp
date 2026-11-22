#include <basalt/wire/wasm/Value.hpp>

#include <doctest.h>

namespace basalt::wire::wasm::test {
namespace {
std::span<const std::uint8_t> view(const std::string& text) {
    return {reinterpret_cast<const std::uint8_t*>(text.data()), text.size()};
}
} // namespace

TEST_CASE("a structured value round-trips and the limits are enforced") {
    const Value value{Value::Map{
        {"name", "stone"},
        {"count", -64},
        {"ratio", 0.5},
        {"flags", Value::List{true, false, Value{}, Blob{{1, 2, 3}}}},
        {"nested", Value::Map{{"deep", std::int64_t{INT64_MIN}}}},
    }};
    std::string bytes;
    REQUIRE(encodeValue(value, bytes));
    const auto back = decodeValue(view(bytes));
    REQUIRE(back.has_value());
    CHECK(*back == value);
    CHECK(back->find("count") != nullptr);

    const std::string integer{"\x03\x7f", 2};
    CHECK(*decodeValue(view(integer))->asInt() == -64);

    CHECK_FALSE(decodeValue(view(bytes.substr(0, bytes.size() - 1))).has_value());
    CHECK_FALSE(decodeValue(view(std::string{"\x07\xff\xff\x03", 4})).has_value());
    CHECK_FALSE(decodeValue(view(std::string{"\x08\x02\x01k\x00\x01k\x00", 8})).has_value());

    Value deep;
    for (std::size_t level = 0; level <= kValueMaxDepth; ++level) {
        deep = Value::List{std::move(deep)};
    }
    std::string rejected = "kept";
    CHECK_FALSE(encodeValue(deep, rejected));
    CHECK(rejected == "kept");
}

} // namespace basalt::wire::wasm::test
