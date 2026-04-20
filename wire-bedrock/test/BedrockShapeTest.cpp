
#include <basalt/wire/bedrock/BedrockPackets.hpp>
#include <basalt/wire/bedrock/BedrockTypes.hpp>

#include <doctest.h>

#include <cstdint>
#include <string>
#include <vector>

using namespace basalt::wire;
using namespace basalt::wire::bedrock;

namespace {

constexpr int kFlagged = 2169;
constexpr int kNative = kNativeBedrockProtocol;

/// `count` presence flags, all on, the way a client announces a field.
void flags(io::ByteBuffer& buffer, const int count) {
    for (int at = 0; at < count; ++at) {
        buffer.writeByte(1);
    }
}

void air(io::ByteBuffer& buffer) {
    writeShortLE(buffer, 0);
    writeShortLE(buffer, 0);
    writeVarUInt(buffer, 0);
    buffer.writeByte(0);
    writeVarUInt(buffer, 0);
    writeVarUInt(buffer, 0);
}

void action(io::ByteBuffer& buffer, const int sourceFlags) {
    writeVarUInt(buffer, 0);
    flags(buffer, sourceFlags);
    buffer.writeByte(7);
    buffer.writeByte(0);
    writeVarUInt(buffer, 3);
    air(buffer);
    air(buffer);
}

std::vector<std::uint8_t> input(const int protocol) {
    const bool single = protocol >= kSingleFlagProtocol;
    const int pair = single ? 1 : 2;
    io::ByteBuffer body;
    for (const float value : {10.0F, 20.0F, 1.0F, 2.0F, 3.0F, 0.0F, 0.0F, 30.0F}) {
        writeFloatLE(body, value);
    }
    if (!single) {
        body.writeByte(1);
    }
    writeVarUInt(body, 3);
    for (const std::int32_t flag : {8, 34, 35}) {
        writeVarInt(body, flag);
    }
    writeVarUInt(body, 0);
    writeVarUInt(body, 0);
    writeVarUInt(body, 0);
    writeFloatLE(body, 0.0F);
    writeFloatLE(body, 0.0F);
    writeVarULong(body, 99);
    for (int at = 0; at < 3; ++at) {
        writeFloatLE(body, 0.0F);
    }

    flags(body, pair);
    writeVarInt(body, 0);
    body.writeByte(0);
    if (!single) {
        flags(body, 2);
    }
    writeVarUInt(body, 1);
    action(body, pair);
    writeVarInt(body, 0);
    body.writeByte(0);
    writeVarInt(body, 4);
    writeVarUInt(body, 5);
    writeVarInt(body, 6);
    body.writeByte(1);
    writeVarInt(body, 2);
    if (single) {
        body.writeByte(0);
    }
    air(body);
    for (int at = 0; at < 3; ++at) {
        writeFloatLE(body, 0.0F);
    }
    writeFloatLE(body, 0.25F);
    writeFloatLE(body, 1.0F);
    writeFloatLE(body, 0.75F);
    writeVarUInt(body, 0);
    body.writeByte(0);
    body.writeByte(0);

    body.writeByte(0);
    flags(body, pair);
    writeVarUInt(body, 1);
    writeVarInt(body, 0);
    writeVarInt(body, -1);
    writeVarInt(body, 64);
    writeVarInt(body, 9);
    writeVarInt(body, 1);
    return body.toVector();
}

} // namespace

TEST_CASE("a tick's input reads the same in the doubled and the single flag shapes") {
    for (const int protocol : {kFlagged, kNative}) {
        CAPTURE(protocol);
        io::ByteBuffer body = io::ByteBuffer::wrap(input(protocol));
        const std::optional<PlayerAuthInput> read = readPlayerAuthInput(body, protocol);
        REQUIRE(read.has_value());
        CHECK_FALSE(body.failed());
        CHECK(body.readableBytes() == 0);
        CHECK(read->sneaking);
        CHECK(read->x == 1.0F);
        CHECK(read->headYaw == 30.0F);
        REQUIRE(read->itemUse.has_value());
        CHECK(read->itemUse->x == 4);
        CHECK(read->itemUse->y == 5);
        CHECK(read->itemUse->z == 6);
        CHECK(read->itemUse->face == 1);
        CHECK(read->itemUse->hotbarSlot == 2);
        CHECK(read->itemUse->clickZ == 0.75F);
        REQUIRE(read->blockActions.size() == 1);
        CHECK(read->blockActions[0].y == 64);
        CHECK(read->blockActions[0].face == 1);
    }
}

TEST_CASE("an answered stack request carries one flag per optional field from 26.50") {
    const std::vector<BedrockResponseContainer> containers{{.container = 12, .slots = {{3, 5, 77}}}};
    for (const int protocol : {kFlagged, kNative}) {
        CAPTURE(protocol);
        const int pair = protocol >= kSingleFlagProtocol ? 1 : 2;
        io::ByteBuffer body;
        writeItemStackResponse(body, 9, true, containers, protocol);

        CHECK(readVarUInt(body) == 1);
        CHECK(body.readByte() == 0);
        CHECK(readVarInt(body) == 9);
        for (int at = 0; at < pair; ++at) {
            CHECK(body.readByte() == 1);
        }
        CHECK(readVarUInt(body) == 1);
        CHECK(body.readByte() == 12);
        CHECK(body.readByte() == 0);
        CHECK(readVarUInt(body) == 1);
        CHECK(body.readByte() == 3);
        CHECK(body.readByte() == 3);
        CHECK(body.readByte() == 5);
        for (int at = 0; at < pair; ++at) {
            CHECK(body.readByte() == 1);
        }
        CHECK(readVarInt(body) == 77);
        CHECK(readString(body).empty());
        CHECK(body.readByte() == 0);
        CHECK(readVarInt(body) == 0);
        CHECK(body.readableBytes() == 0);
        CHECK_FALSE(body.failed());
    }
}

TEST_CASE("a score line names its kind itself from 26.40 on") {
    const std::vector<BedrockScoreLine> lines{{.lineId = 7, .name = "Line", .score = 3}};
    const std::vector<std::int64_t> gone{7};
    for (const int protocol : {1001, 2168, kNative}) {
        CAPTURE(protocol);
        const bool named = protocol >= kScoreKindProtocol;
        io::ByteBuffer set;
        writeSetScore(set, "board", lines, protocol);
        if (!named) {
            CHECK(set.readByte() == 0);
        }
        CHECK(readVarUInt(set) == 1);
        if (named) {
            CHECK(readVarUInt(set) == 3);
            CHECK(readString(set) == "changefakeplayer");
        }
        CHECK(readVarLong(set) == 7);
        CHECK(readString(set) == "board");
        CHECK(readIntLE(set) == 3);
        if (!named) {
            CHECK(set.readByte() == 3);
        }
        CHECK(readString(set) == "Line");
        CHECK(set.readableBytes() == 0);

        io::ByteBuffer removed;
        writeRemoveScores(removed, "board", gone, protocol);
        if (!named) {
            CHECK(removed.readByte() == 1);
        }
        CHECK(readVarUInt(removed) == 1);
        if (named) {
            CHECK(readVarUInt(removed) == 0);
            CHECK(readString(removed) == "remove");
        }
        CHECK(readVarLong(removed) == 7);
        if (named) {
            CHECK(removed.readByte() == 0);
        } else {
            CHECK(readString(removed) == "board");
            CHECK(readIntLE(removed) == 0);
        }
        CHECK(removed.readableBytes() == 0);
        CHECK_FALSE(removed.failed());
    }
}
