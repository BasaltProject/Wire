#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <basalt/wire/nbt/NbtIO.hpp>
#include <basalt/wire/nbt/NbtTag.hpp>
#include <basalt/wire/nbt/Snbt.hpp>

#include <doctest.h>

#include <cstdint>
#include <string>
#include <vector>

using namespace basalt::wire::nbt;

namespace {

const std::string kTricky = std::string("a\0b", 3) + "\xF0\x9F\x98\x80" + "c";

NbtCompound buildSample() {
    NbtCompound root;
    root.putByte("byte", -12);
    root.putBoolean("flag", true);
    root.putShort("short", 1234);
    root.putInt("int", -70000);
    root.putLong("long", 1234567890123LL);
    root.putFloat("float", 3.14F);
    root.putDouble("double", 2.718281828459045);
    root.putString("string", "hello world");
    root.putByteArray("byteArray", {1, 2, 3, -1});
    root.putIntArray("intArray", {1, -2, 3, 400000});
    root.putLongArray("longArray", {1LL, -2LL, 9999999999LL});

    NbtCompound nested;
    nested.putString("inner", "value");
    root.set("nested", NbtTag::ofCompound(std::move(nested)));

    root.set("listOfInts",
             NbtTag::ofList(NbtList{NbtType::Int, {NbtTag::ofInt(1), NbtTag::ofInt(2), NbtTag::ofInt(3)}}));

    NbtCompound first;
    first.putInt("id", 1);
    NbtCompound second;
    second.putInt("id", 2);
    root.set("listOfCompounds",
             NbtTag::ofList(NbtList{NbtType::Compound,
                                    {NbtTag::ofCompound(std::move(first)), NbtTag::ofCompound(std::move(second))}}));

    root.set("emptyList", NbtTag::ofList(NbtList{}));
    root.putString("tricky", kTricky);
    return root;
}

std::string hex(std::span<const std::uint8_t> bytes) {
    static constexpr char kDigits[] = "0123456789abcdef";
    std::string out;
    out.reserve(bytes.size() * 2);
    for (const std::uint8_t byte : bytes) {
        out += kDigits[byte >> 4];
        out += kDigits[byte & 0x0F];
    }
    return out;
}

} // namespace

constexpr std::string_view kGolden =
    "0a0004526f6f7401000462797465f4010004666c61670102000573686f727404d2030003696e74fffeee900400046c6f6e670000011f71fb"
    "04cb050005666c6f61744048f5c3060006646f75626c654005bf0a8b145769080006737472696e67000b68656c6c6f20776f726c64070009"
    "62797465417272617900000004010203ff0b0008696e7441727261790000000400000001fffffffe0000000300061a800c00096c6f6e6741"
    "72726179000000030000000000000001fffffffffffffffe00000002540be3ff0a00066e6573746564080005696e6e6572000576616c7565"
    "0009000a6c6973744f66496e7473030000000300000001000000020000000309000f6c6973744f66436f6d706f756e64730a000000020300"
    "026964000000010003000269640000000200090009656d7074794c6973740000000000080006747269636b79000b61c08062eda0bdedb880"
    "6300";

TEST_CASE("binary NBT is byte-identical to the frozen oracle") {
    const std::vector<std::uint8_t> bytes = nbtIO::write(NbtTag::ofCompound(buildSample()), "Root");
    CHECK(hex(bytes) == kGolden);
}

TEST_CASE("binary round trip preserves every tag type") {
    const NbtCompound sample = buildSample();
    const std::vector<std::uint8_t> bytes = nbtIO::write(NbtTag::ofCompound(sample), "Root");

    const auto parsed = nbtIO::read(bytes);
    REQUIRE(parsed.has_value());
    CHECK(parsed->name == "Root");
    REQUIRE(parsed->tag.type() == NbtType::Compound);
    CHECK(*parsed->tag.get<NbtCompound>() == sample);
    CHECK(parsed->tag.get<NbtCompound>()->getString("tricky") == kTricky);
}

TEST_CASE("anonymous NBT drops the root name field") {
    const NbtTag sample = NbtTag::ofCompound(buildSample());
    const std::vector<std::uint8_t> anonymous = nbtIO::writeAnonymous(sample);
    const std::vector<std::uint8_t> named = nbtIO::write(sample, "");
    CHECK(anonymous.size() + 2 == named.size());

    const auto parsed = nbtIO::readAnonymous(anonymous);
    REQUIRE(parsed.has_value());
    CHECK(parsed->tag == sample);
    CHECK(parsed->bytesConsumed == anonymous.size());
}

TEST_CASE("malformed NBT is refused rather than thrown out of the parser") {
    CHECK(nbtIO::read(std::vector<std::uint8_t>{99, 0, 0}).error() == NbtError::UnknownTagId);
    CHECK(nbtIO::read(std::vector<std::uint8_t>{}).error() == NbtError::UnexpectedEnd);
    CHECK(nbtIO::readAnonymous(std::vector<std::uint8_t>{7, 0x7F, 0xFF, 0xFF, 0xFF}).error() ==
          NbtError::UnexpectedEnd);
    std::vector<std::uint8_t> deep;
    for (int i = 0; i < nbtIO::kMaxDepth + 8; ++i) {
        deep.insert(deep.end(), {9, 9, 0, 0, 0, 1});
    }
    CHECK_FALSE(nbtIO::readAnonymous(deep).has_value());
}

TEST_CASE("SNBT stringify then parse round trips a complex tree") {
    using namespace basalt::wire::nbt::snbt;
    NbtCompound root;
    root.putByte("byte", -12);
    root.putShort("short", 1234);
    root.putInt("int", -70000);
    root.putLong("long", 1234567890123LL);
    root.putFloat("float", 3.25F);
    root.putDouble("double", 2.71828);
    root.putString("plainKey", "unquoted_value");
    root.putString("key with spaces", "needs quoting");
    root.putIntArray("ints", {1, -2, 3});
    root.set("list", NbtTag::ofList(NbtList{NbtType::String, {NbtTag::ofString("a"), NbtTag::ofString("b")}}));

    NbtCompound nested;
    nested.putBoolean("flag", true);
    root.set("nested", NbtTag::ofCompound(std::move(nested)));

    const NbtTag tag = NbtTag::ofCompound(root);
    const auto reparsed = parse(stringify(tag));
    REQUIRE(reparsed.has_value());
    CHECK(*reparsed == tag);
}
