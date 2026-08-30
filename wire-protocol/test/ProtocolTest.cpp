#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <basalt/wire/common/Base64.hpp>
#include <basalt/wire/protocol/PacketFraming.hpp>
#include <basalt/wire/protocol/ProtocolBuffer.hpp>
#include <basalt/wire/protocol/VarInt.hpp>
#include <basalt/wire/protocol/handshake/HandshakePacket.hpp>
#include <basalt/wire/protocol/login/LoginPackets.hpp>
#include <basalt/wire/protocol/play/BlockPos.hpp>
#include <basalt/wire/protocol/play/LowPrecisionVec3.hpp>
#include <basalt/wire/protocol/play/NibbleArray.hpp>
#include <basalt/wire/protocol/play/PalettedContainer.hpp>
#include <basalt/wire/protocol/status/StatusPackets.hpp>

#include <doctest.h>

#include <string>
#include <vector>

using namespace basalt::wire;
using namespace basalt::wire::protocol;

namespace {

std::vector<std::uint8_t> encodeVarInt(std::int32_t value) {
    io::ByteBuffer buffer;
    varInt::write(buffer, value);
    return buffer.toVector();
}

} // namespace

TEST_CASE("VarInt matches the vanilla encoding") {
    CHECK(encodeVarInt(0) == std::vector<std::uint8_t>{0x00});
    CHECK(encodeVarInt(1) == std::vector<std::uint8_t>{0x01});
    CHECK(encodeVarInt(127) == std::vector<std::uint8_t>{0x7F});
    CHECK(encodeVarInt(128) == std::vector<std::uint8_t>{0x80, 0x01});
    CHECK(encodeVarInt(2097151) == std::vector<std::uint8_t>{0xFF, 0xFF, 0x7F});
    CHECK(encodeVarInt(2147483647) == std::vector<std::uint8_t>{0xFF, 0xFF, 0xFF, 0xFF, 0x07});
    // A negative value takes the full five bytes: there is no zigzag in this encoding.
    CHECK(encodeVarInt(-1) == std::vector<std::uint8_t>{0xFF, 0xFF, 0xFF, 0xFF, 0x0F});
    CHECK(varInt::sizeOf(0) == 1);
    CHECK(varInt::sizeOf(128) == 2);
    CHECK(varInt::sizeOf(-1) == 5);

    for (const std::int32_t value : std::vector<std::int32_t>{0, 1, -1, 127, 128, 2147483647, -2147483647 - 1}) {
        io::ByteBuffer buffer = io::ByteBuffer::wrap(encodeVarInt(value));
        CHECK(varInt::read(buffer) == value);
        CHECK_FALSE(buffer.failed());
    }
}

TEST_CASE("VarLong round trips and refuses an over-long encoding") {
    for (const std::int64_t value : std::vector<std::int64_t>{0, 1, -1, 9223372036854775807LL}) {
        io::ByteBuffer buffer;
        varLong::write(buffer, value);
        CHECK(varLong::read(buffer) == value);
        CHECK_FALSE(buffer.failed());
    }

    io::ByteBuffer tooLong = io::ByteBuffer::wrap(std::vector<std::uint8_t>(12, 0xFF));
    static_cast<void>(varLong::read(tooLong));
    CHECK(tooLong.failed());
}

TEST_CASE("a malformed VarInt fails the buffer instead of throwing") {
    io::ByteBuffer tooLong = io::ByteBuffer::wrap(std::vector<std::uint8_t>{0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x01});
    static_cast<void>(varInt::read(tooLong));
    CHECK(tooLong.failed());

    io::ByteBuffer truncated = io::ByteBuffer::wrap(std::vector<std::uint8_t>{0x80});
    static_cast<void>(varInt::read(truncated));
    CHECK(truncated.failed());
}

TEST_CASE("protocol fields round trip") {
    io::ByteBuffer buffer;
    writeString(buffer, "Basalt");
    writeBoolean(buffer, true);
    const std::vector<std::uint8_t> payload = {9, 8, 7};
    writeByteArray(buffer, payload);
    const common::Uuid uuid{0x0123456789ABCDEFULL, 0xFEDCBA9876543210ULL};
    writeUuid(buffer, uuid);

    CHECK(readString(buffer) == "Basalt");
    CHECK(readBoolean(buffer));
    CHECK(readByteArray(buffer) == payload);
    CHECK(readUuid(buffer) == uuid);
    CHECK_FALSE(buffer.failed());
}

TEST_CASE("a string longer than its declared maximum is refused") {
    io::ByteBuffer buffer;
    writeString(buffer, std::string(64, 'x'));
    CHECK(readString(buffer, 8).empty());
    CHECK(buffer.failed());
}

TEST_CASE("a collection length larger than the packet is refused") {
    io::ByteBuffer buffer;
    varInt::write(buffer, 1000000);
    buffer.writeByte(1);
    CHECK(readCollectionSize(buffer) == 0);
    CHECK(buffer.failed());
}

TEST_CASE("uncompressed framing round trips") {
    const std::vector<std::uint8_t> packet = {0x00, 'h', 'i'};
    io::ByteBuffer wire;
    packetFraming::writeUncompressed(wire, packet);

    io::ByteBuffer frame;
    const auto got = packetFraming::tryReadFrame(wire, false, frame);
    REQUIRE(got.has_value());
    CHECK(*got);
    CHECK(frame.toVector() == packet);
}

TEST_CASE("a partial frame is left alone until the rest of it arrives") {
    const std::vector<std::uint8_t> packet(200, 0x42);
    io::ByteBuffer complete;
    packetFraming::writeUncompressed(complete, packet);
    const std::vector<std::uint8_t> bytes = complete.toVector();

    io::ByteBuffer partial;
    partial.writeBytes(std::span{bytes}.first(50));
    io::ByteBuffer frame;
    CHECK_FALSE(*packetFraming::tryReadFrame(partial, false, frame));
    CHECK(partial.readableBytes() == 50);

    partial.writeBytes(std::span{bytes}.subspan(50));
    CHECK(*packetFraming::tryReadFrame(partial, false, frame));
    CHECK(frame.toVector() == packet);
}

TEST_CASE("compressed framing round trips on both sides of the threshold") {
    io::ByteBuffer frame;

    const std::vector<std::uint8_t> small = {0x00, 1, 2, 3};
    io::ByteBuffer wireSmall;
    REQUIRE(packetFraming::writeCompressed(wireSmall, small, 256).has_value());
    REQUIRE(*packetFraming::tryReadFrame(wireSmall, true, frame));
    CHECK(frame.toVector() == small);
    CHECK(wireSmall.readableBytes() == 0);

    std::vector<std::uint8_t> large(4096);
    for (std::size_t i = 0; i < large.size(); ++i) {
        large[i] = static_cast<std::uint8_t>(i % 97);
    }
    io::ByteBuffer wireLarge;
    REQUIRE(packetFraming::writeCompressed(wireLarge, large, 256).has_value());
    CHECK(wireLarge.readableBytes() < large.size());
    REQUIRE(*packetFraming::tryReadFrame(wireLarge, true, frame));
    CHECK(frame.toVector() == large);
    CHECK(wireLarge.readableBytes() == 0);
}

TEST_CASE("a frame claiming more than the protocol allows is refused") {
    io::ByteBuffer wire;
    varInt::write(wire, packetFraming::kMaxUncompressedSize + 1);
    io::ByteBuffer frame;
    CHECK(packetFraming::tryReadFrame(wire, false, frame).error() == FramingError::FrameTooBig);
}

TEST_CASE("BlockPos packs the way vanilla does") {
    using namespace basalt::wire::protocol::play;
    for (const BlockPos position : std::vector<BlockPos>{
             {0, 0, 0}, {1, 2, 3}, {-1, -1, -1}, {30000000, 320, -30000000}, {-30000000, -64, 30000000}}) {
        CHECK(BlockPos::unpack(position.pack()) == position);
    }
    CHECK(BlockPos{1, 2, 3}.offset(Direction::Up) == BlockPos{1, 3, 3});
    CHECK(BlockPos{1, 2, 3}.offset(Direction::West) == BlockPos{0, 2, 3});
}

TEST_CASE("BitStorage never lets an entry span two words") {
    using namespace basalt::wire::protocol::play;
    BitStorage storage{5, 100};
    // 12 entries per word at five bits, so 100 entries need 9 words with padding in each.
    CHECK(storage.words().size() == 9);
    for (std::size_t i = 0; i < 100; ++i) {
        storage.set(i, static_cast<std::int32_t>(i % 32));
    }
    for (std::size_t i = 0; i < 100; ++i) {
        CHECK(storage.get(i) == static_cast<std::int32_t>(i % 32));
    }
}

TEST_CASE("a paletted section round trips through all three palette kinds") {
    using namespace basalt::wire::protocol::play;

    const auto roundTrip = [](const std::vector<std::int32_t>& values) {
        io::ByteBuffer buffer;
        palettedContainer::write(buffer, values, palettedContainer::kBlockMinIndirectBits,
                                 palettedContainer::kBlockMaxIndirectBits, 15);
        const auto read = palettedContainer::read(buffer, values.size(), palettedContainer::kBlockMaxIndirectBits);
        CHECK_FALSE(buffer.failed());
        CHECK(buffer.readableBytes() == 0);
        return read;
    };

    const std::vector<std::int32_t> single(4096, 7);
    CHECK(roundTrip(single) == single);

    std::vector<std::int32_t> indirect(4096);
    for (std::size_t i = 0; i < indirect.size(); ++i) {
        indirect[i] = static_cast<std::int32_t>(i % 9);
    }
    CHECK(roundTrip(indirect) == indirect);

    std::vector<std::int32_t> direct(4096);
    for (std::size_t i = 0; i < direct.size(); ++i) {
        direct[i] = static_cast<std::int32_t>(i);
    }
    CHECK(roundTrip(direct) == direct);
}

TEST_CASE("nibble packing round trips a whole section") {
    using namespace basalt::wire::protocol::play;
    std::vector<std::int32_t> levels(4096);
    for (std::size_t i = 0; i < levels.size(); ++i) {
        levels[i] = static_cast<std::int32_t>(i % 16);
    }
    std::vector<std::uint8_t> packed(nibbleArray::kSectionByteCount);
    nibbleArray::pack(levels, packed);

    std::vector<std::int32_t> restored(4096);
    nibbleArray::unpack(packed, restored);
    CHECK(restored == levels);
    CHECK(nibbleArray::uniform(15).front() == 0xFF);
}

TEST_CASE("the low precision vector keeps the direction and costs one byte for nothing") {
    using namespace basalt::wire::protocol::play;
    io::ByteBuffer zero;
    lowPrecisionVec3::write(zero, 0.0, 0.0, 0.0);
    CHECK(zero.readableBytes() == 1);
    const auto readZero = lowPrecisionVec3::read(zero);
    CHECK(readZero[0] == 0.0);

    io::ByteBuffer buffer;
    lowPrecisionVec3::write(buffer, 0.5, -1.25, 2.0);
    const auto read = lowPrecisionVec3::read(buffer);
    CHECK(read[0] == doctest::Approx(0.5).epsilon(1e-3));
    CHECK(read[1] == doctest::Approx(-1.25).epsilon(1e-3));
    CHECK(read[2] == doctest::Approx(2.0).epsilon(1e-3));
    CHECK_FALSE(buffer.failed());
}

TEST_CASE("the handshake packet round trips and refuses an unknown intent") {
    using namespace basalt::wire::protocol::handshake;
    const HandshakePacket packet{776, "play.basaltmc.org", 25565, ClientIntent::Login};

    io::ByteBuffer buffer;
    packet.write(buffer);
    const auto read = HandshakePacket::read(buffer);
    REQUIRE(read.has_value());
    CHECK(*read == packet);

    io::ByteBuffer bad;
    varInt::write(bad, 776);
    writeString(bad, "host");
    bad.writeShort(25565);
    varInt::write(bad, 99);
    CHECK_FALSE(HandshakePacket::read(bad).has_value());
}

TEST_CASE("the login packets round trip") {
    using namespace basalt::wire::protocol::login;

    const LoginStartPacket start{"Steve", common::Uuid::offlinePlayer("Steve")};
    io::ByteBuffer startBuffer;
    start.write(startBuffer);
    CHECK(*LoginStartPacket::read(startBuffer) == start);

    const HelloPacket hello{"", {1, 2, 3, 4, 5}, {9, 9, 9, 9}, true};
    io::ByteBuffer helloBuffer;
    hello.write(helloBuffer);
    CHECK(*HelloPacket::read(helloBuffer) == hello);

    const KeyPacket key{{7, 7, 7}, {8, 8}};
    io::ByteBuffer keyBuffer;
    key.write(keyBuffer);
    CHECK(*KeyPacket::read(keyBuffer) == key);

    const LoginCompressionPacket compression{256};
    io::ByteBuffer compressionBuffer;
    compression.write(compressionBuffer);
    CHECK(*LoginCompressionPacket::read(compressionBuffer) == compression);

    LoginFinishedPacket finished;
    finished.gameProfile.id = common::Uuid::offlinePlayer("Steve");
    finished.gameProfile.name = "Steve";
    finished.gameProfile.properties.push_back(GameProfileProperty{"textures", "base64value", "signed"});
    finished.gameProfile.properties.push_back(GameProfileProperty{"other", "value", std::nullopt});
    finished.sessionId = common::Uuid{1, 2};
    io::ByteBuffer finishedBuffer;
    finished.write(finishedBuffer);
    const auto readFinished = LoginFinishedPacket::read(finishedBuffer);
    REQUIRE(readFinished.has_value());
    CHECK(*readFinished == finished);
}

TEST_CASE("the offline-mode UUID matches what vanilla derives") {
    // parity: UUID.nameUUIDFromBytes("OfflinePlayer:<name>"), the id every offline server hands
    // out. The values are the version 3 UUIDs of those exact strings, derived rather than recalled.
    CHECK(common::Uuid::offlinePlayer("Steve").toString() == "5627dd98-e6be-3c21-b8a8-e92344183641");
    CHECK(common::Uuid::offlinePlayer("Alex").toString() == "36532b5e-c442-3dbb-a24c-c7e55d0f979a");
    CHECK(common::Uuid::offlinePlayer("Notch").toString() == "b50ad385-829d-3141-a216-7e7d7539ba7f");
    CHECK(common::Uuid::parse("5627dd98-e6be-3c21-b8a8-e92344183641") == common::Uuid::offlinePlayer("Steve"));
    CHECK(common::Uuid::offlinePlayer("Steve").toUndashedString() == "5627dd98e6be3c21b8a8e92344183641");
}

TEST_CASE("the status payload round trips through its JSON body") {
    using namespace basalt::wire::protocol::status;
    ServerStatusPayload payload;
    payload.description = text::Component::text("A Basalt server", text::kGold);
    payload.players = ServerStatusPayload::Players{20, 1, {{"Steve", "8667ba71-b85a-4004-af54-457a9734eed7"}}};
    payload.version = ServerStatusPayload::Version{"Basalt 26.2", 776};
    payload.favicon = std::vector<std::uint8_t>{0x89, 'P', 'N', 'G'};
    payload.enforcesSecureChat = true;

    const auto reparsed = ServerStatusPayload::fromJsonString(payload.toJsonString());
    REQUIRE(reparsed.has_value());
    CHECK(*reparsed == payload);

    io::ByteBuffer buffer;
    StatusResponsePacket{payload}.write(buffer);
    const auto packet = StatusResponsePacket::read(buffer);
    REQUIRE(packet.has_value());
    CHECK(packet->status == payload);
}

TEST_CASE("base64 round trips every remainder") {
    for (std::size_t size = 0; size < 8; ++size) {
        std::vector<std::uint8_t> bytes(size);
        for (std::size_t i = 0; i < size; ++i) {
            bytes[i] = static_cast<std::uint8_t>(i * 37);
        }
        CHECK(common::base64::decode(common::base64::encode(bytes)).value() == bytes);
    }
    CHECK(common::base64::encode(std::vector<std::uint8_t>{'M'}) == "TQ==");
    CHECK(common::base64::encode(std::vector<std::uint8_t>{'M', 'a'}) == "TWE=");
    CHECK(common::base64::encode(std::vector<std::uint8_t>{'M', 'a', 'n'}) == "TWFu");
    CHECK_FALSE(common::base64::decode("not base64!").has_value());
}
