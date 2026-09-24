// The packets whose shape moves inside the window, each read or written in every shape it has there.
// The bytes are laid out by hand from the protocol, so a codec that drifts from it fails here.

#include <basalt/wire/bedrock/BedrockPackets.hpp>
#include <basalt/wire/bedrock/BedrockTypes.hpp>

#include <doctest.h>

#include <cstdint>
#include <string>
#include <vector>

using namespace basalt::wire;
using namespace basalt::wire::bedrock;

namespace {

constexpr int kFlagged = 2169; // The last protocol with 2168's doubled flags.
constexpr int kNative = kNativeBedrockProtocol;

/// `count` presence flags, all on, the way a client announces a field.
void flags(io::ByteBuffer& buffer, const int count) {
    for (int at = 0; at < count; ++at) {
        buffer.writeByte(1);
    }
}

/// An empty stack in the descriptor shape both protocols use for a held item.
void air(io::ByteBuffer& buffer) {
    writeShortLE(buffer, 0);
    writeShortLE(buffer, 0);
    writeVarUInt(buffer, 0);
    buffer.writeByte(0);
    writeVarUInt(buffer, 0);
    writeVarUInt(buffer, 0);
}

/// One moved stack from a window, with its source in the shape the protocol announces it.
void action(io::ByteBuffer& buffer, const int sourceFlags) {
    writeVarUInt(buffer, 0); // From a window,
    flags(buffer, sourceFlags);
    buffer.writeByte(7); // number seven,
    buffer.writeByte(0); // and nothing the world did.
    writeVarUInt(buffer, 3);
    air(buffer);
    air(buffer);
}

/// A tick in which the player crouches, clicks a block and starts breaking another.
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

    flags(body, pair); // The click,
    writeVarInt(body, 0);
    body.writeByte(0); // no old slots,
    if (!single) {
        flags(body, 2);
    }
    writeVarUInt(body, 1);
    action(body, pair);
    writeVarInt(body, 0);  // on a block,
    body.writeByte(0);     // by a hand,
    writeVarInt(body, 4);  // at 4,
    writeVarUInt(body, 5); // 5,
    writeVarInt(body, 6);  // 6,
    body.writeByte(1);     // on top,
    writeVarInt(body, 2);  // holding slot two,
    if (single) {
        body.writeByte(0); // in the main hand.
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

    body.writeByte(0); // No stack request.
    flags(body, pair); // One block action.
    writeVarUInt(body, 1);
    writeVarInt(body, 0);
    writeVarInt(body, -1);
    writeVarInt(body, 64);
    writeVarInt(body, 9);
    writeVarInt(body, 1);
    return body.toVector();
}

/// A hit on an entity, with one moved stack in front of it.
std::vector<std::uint8_t> hit(const int protocol) {
    const bool single = protocol >= kSingleFlagProtocol;
    io::ByteBuffer body;
    writeVarInt(body, 0);
    body.writeByte(0); // No old slots.
    if (!single) {
        body.writeByte(1);
    }
    writeVarUInt(body, 3); // A use on an entity.
    if (!single) {
        body.writeByte(1);
    }
    writeVarUInt(body, 1);
    action(body, single ? 1 : 2);
    writeVarULong(body, 42);
    writeVarInt(body, 1);
    writeVarInt(body, 5);
    air(body);
    for (int at = 0; at < 6; ++at) {
        writeFloatLE(body, 0.0F);
    }
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

TEST_CASE("a hit on an entity reads past the moved stacks in both shapes") {
    for (const int protocol : {kFlagged, kNative}) {
        CAPTURE(protocol);
        io::ByteBuffer body = io::ByteBuffer::wrap(hit(protocol));
        const BedrockTransaction read = readTransaction(body, protocol);
        REQUIRE(read.entityUse.has_value());
        CHECK(read.entityUse->runtimeEntityId == 42);
        CHECK(read.entityUse->action == 1);
        CHECK(read.entityUse->hotbarSlot == 5);
        CHECK(body.readableBytes() == 0);
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

TEST_CASE("a played sound grew two trailing fields in 26.50") {
    for (const int protocol : {kFlagged, kNative}) {
        CAPTURE(protocol);
        io::ByteBuffer body;
        writePlaySound(body, "random.click", 1.0, 2.0, 3.0, 0.5F, 1.5F, protocol);
        CHECK(readString(body) == "random.click");
        CHECK(readVarInt(body) == 8);
        CHECK(readVarUInt(body) == 16);
        CHECK(readVarInt(body) == 24);
        CHECK(readFloatLE(body) == 0.5F);
        CHECK(readFloatLE(body) == 1.5F);
        CHECK(readVarUInt(body) == 0);
        const std::size_t rest = protocol >= kSoundPlaybackProtocol ? 3 : 1;
        CHECK(body.readableBytes() == rest);
        for (std::size_t at = 0; at < rest; ++at) {
            CHECK(body.readByte() == 0);
        }
    }
}

TEST_CASE("a boss bar stopped naming its player in 26.50") {
    for (const int protocol : {kFlagged, kNative}) {
        CAPTURE(protocol);
        io::ByteBuffer body;
        writeBossEvent(body, 5, BedrockBossAction::Create, "Boss", 0.5F, protocol);
        CHECK(readVarLong(body) == 5);
        if (protocol < kBossBarPlayerlessProtocol) {
            CHECK(readVarLong(body) == 0);
        }
        CHECK(body.readByte() == static_cast<std::uint8_t>(BedrockBossAction::Create));
        CHECK(readString(body) == "Boss");
        CHECK(readString(body).empty());
        CHECK(readFloatLE(body) == 0.5F);
        CHECK(body.readByte() == 0);
        CHECK(body.readByte() == 0);
        CHECK(body.readableBytes() == 0);
    }
}

TEST_CASE("a camera preset grew a starting rotation in 26.50") {
    for (const int protocol : {kFlagged, kNative}) {
        CAPTURE(protocol);
        io::ByteBuffer body;
        writeCameraPresets(body, protocol);
        const std::uint32_t presets = readVarUInt(body);
        CHECK(presets == 4);
        const int trailing = 20 + (protocol >= kCameraStartRotationProtocol ? 2 : 0);
        for (std::uint32_t preset = 0; preset < presets; ++preset) {
            CHECK(readString(body).starts_with("minecraft:"));
            CHECK(readString(body).empty());
            for (int at = 0; at < trailing; ++at) {
                CHECK(body.readByte() == 0);
            }
        }
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

TEST_CASE("a 26.50 jukebox plays its record under a handle") {
    io::ByteBuffer sound;
    writePlaySound(sound, "record.cat", 1.5, 2.5, 3.5, 4.0F, 1.0F, kNative, 9);
    CHECK(readString(sound) == "record.cat");
    CHECK(readVarInt(sound) == 12);
    CHECK(readVarUInt(sound) == 20);
    CHECK(readVarInt(sound) == 28);
    CHECK(readFloatLE(sound) == 4.0F);
    CHECK(readFloatLE(sound) == 1.0F);
    CHECK(readVarUInt(sound) == 0);
    CHECK(sound.readByte() == 0);
    CHECK(sound.readByte() == 1);
    CHECK(readLongLE(sound) == 9);
    CHECK(sound.readByte() == 0);
    CHECK(sound.readableBytes() == 0);

    io::ByteBuffer started;
    writeRecordStarted(started, -4, -60, 5, 9);
    CHECK(readVarInt(started) == -4);
    CHECK(readVarInt(started) == -60);
    CHECK(readVarInt(started) == 5);
    CHECK(readLongLE(started) == 9);
    CHECK(started.readableBytes() == 0);

    io::ByteBuffer stopped;
    writeStopSoundHandle(stopped, 9);
    CHECK(readLongLE(stopped) == 9);
    for (int slot = 0; slot < 7; ++slot) {
        CHECK(readVarUInt(stopped) == 0);
    }
    CHECK(stopped.readableBytes() == 0);
    CHECK_FALSE(stopped.failed());
}
