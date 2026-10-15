#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <basalt/wire/common/Base64.hpp>
#include <basalt/wire/protocol/PacketFraming.hpp>
#include <basalt/wire/protocol/ProtocolBuffer.hpp>
#include <basalt/wire/protocol/VarInt.hpp>
#include <basalt/wire/protocol/configuration/ConfigurationPackets.hpp>
#include <basalt/wire/protocol/handshake/HandshakePacket.hpp>
#include <basalt/wire/protocol/login/LoginPackets.hpp>
#include <basalt/wire/protocol/login/LoginPluginPackets.hpp>
#include <basalt/wire/protocol/play/AdvancementPackets.hpp>
#include <basalt/wire/protocol/play/BlockEntityPackets.hpp>
#include <basalt/wire/protocol/play/BlockPos.hpp>
#include <basalt/wire/protocol/play/BorderPackets.hpp>
#include <basalt/wire/protocol/play/CommandPackets.hpp>
#include <basalt/wire/protocol/play/EntityData.hpp>
#include <basalt/wire/protocol/play/EntityPackets.hpp>
#include <basalt/wire/protocol/play/HudPackets.hpp>
#include <basalt/wire/protocol/play/InventoryPackets.hpp>
#include <basalt/wire/protocol/play/ItemComponent.hpp>
#include <basalt/wire/protocol/play/LowPrecisionVec3.hpp>
#include <basalt/wire/protocol/play/MapPackets.hpp>
#include <basalt/wire/protocol/play/NibbleArray.hpp>
#include <basalt/wire/protocol/play/PalettedContainer.hpp>
#include <basalt/wire/protocol/play/ParticleOptions.hpp>
#include <basalt/wire/protocol/play/PlayPackets.hpp>
#include <basalt/wire/protocol/play/PlayerStatePackets.hpp>
#include <basalt/wire/protocol/play/RecipePackets.hpp>
#include <basalt/wire/protocol/play/SimplePackets.hpp>
#include <basalt/wire/protocol/play/UiPackets.hpp>
#include <basalt/wire/protocol/play/VehiclePackets.hpp>
#include <basalt/wire/protocol/play/WorldPackets.hpp>
#include <basalt/wire/protocol/status/StatusPackets.hpp>

#include <doctest.h>

#include <array>
#include <cstdint>
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

TEST_CASE("a bit set travels as java.util.BitSet.toByteArray") {
    // Bits 1 and 8: little-endian bytes, with the trailing zero bytes of the word left out.
    io::ByteBuffer buffer;
    const std::vector<std::int64_t> words = {0x102, 0};
    writeBitSet(buffer, words);
    CHECK(buffer.readable().size() == 3);
    CHECK(buffer.readable()[0] == 2);
    CHECK(buffer.readable()[1] == 0x02);
    CHECK(buffer.readable()[2] == 0x01);
    CHECK(readBitSet(buffer) == std::vector<std::int64_t>{0x102});
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
    REQUIRE(packetFraming::writeUncompressed(wire, packet));

    io::ByteBuffer frame;
    const auto got = packetFraming::tryReadFrame(wire, false, frame);
    REQUIRE(got.has_value());
    CHECK(*got);
    CHECK(frame.toVector() == packet);
}

TEST_CASE("a partial frame is left alone until the rest of it arrives") {
    const std::vector<std::uint8_t> packet(200, 0x42);
    io::ByteBuffer complete;
    REQUIRE(packetFraming::writeUncompressed(complete, packet));
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

TEST_CASE("a compressed length cannot consume bytes from the next frame") {
    // First frame contains only an unterminated data-length VarInt. The next frame is valid.
    io::ByteBuffer wire;
    wire.writeBytes(std::array<std::uint8_t, 5>{1, 0x80, 2, 0, 0x42});
    io::ByteBuffer frame;
    const auto malformed = packetFraming::tryReadFrame(wire, true, frame);
    REQUIRE_FALSE(malformed.has_value());
    CHECK(wire.readerIndex() <= 2);
    CHECK(frame.readableBytes() == 0);
}

TEST_CASE("a zero-length frame is refused") {
    io::ByteBuffer wire;
    wire.writeByte(0);
    io::ByteBuffer frame;
    CHECK_FALSE(packetFraming::tryReadFrame(wire, false, frame).has_value());
}

TEST_CASE("frame lengths cannot be encoded wider than twenty-one bits") {
    io::ByteBuffer wire;
    wire.writeBytes(std::array<std::uint8_t, 4>{0x81, 0x80, 0x80, 0});
    wire.writeByte(0x42);
    io::ByteBuffer frame;
    CHECK_FALSE(packetFraming::tryReadFrame(wire, false, frame).has_value());
}

TEST_CASE("compressed packets validate the negotiated threshold and declared size") {
    const std::vector<std::uint8_t> packet(200, 0x42);
    io::ByteBuffer wire;
    REQUIRE(packetFraming::writeCompressed(wire, packet, 0));
    io::ByteBuffer frame;
    const auto below = packetFraming::tryReadFrame(wire, true, frame, 256);
    REQUIRE_FALSE(below.has_value());
    CHECK(below.error() == FramingError::BelowCompressionThreshold);
    CHECK(wire.readerIndex() == 0);
    REQUIRE(*packetFraming::tryReadFrame(wire, true, frame, 0));
    CHECK(frame.toVector() == packet);

    // The compressed bytes really expand to 200, not the forged declared size of 201.
    io::ByteBuffer forged;
    REQUIRE(packetFraming::writeCompressed(forged, packet, 0));
    forged.mutableSpan(0)[1] = 201;
    const auto wrongSize = packetFraming::tryReadFrame(forged, true, frame, 0);
    REQUIRE_FALSE(wrongSize.has_value());
    CHECK(wrongSize.error() == FramingError::DecompressionFailed);
    CHECK(frame.readableBytes() == 0);
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

TEST_CASE("the configuration packets round trip") {
    using namespace basalt::wire::protocol::configuration;

    const ClientInformationPacket information{"en_us", 12, 0, true, 0x7F, 1, false, true, 0};
    io::ByteBuffer informationBuffer;
    information.write(informationBuffer);
    CHECK(*ClientInformationPacket::read(informationBuffer) == information);

    const PostEffectsPacket effects{{"minecraft:spider", "minecraft:creeper"}};
    io::ByteBuffer effectsBuffer;
    effects.write(effectsBuffer);
    CHECK(*PostEffectsPacket::read(effectsBuffer) == effects);

    const SelectKnownPacksPacket packs{{{"minecraft", "core", "26.3"}}};
    io::ByteBuffer packsBuffer;
    packs.write(packsBuffer);
    CHECK(*SelectKnownPacksPacket::read(packsBuffer) == packs);
    // Basalt sends none of its own, so the empty list has to survive too.
    io::ByteBuffer emptyPacks;
    SelectKnownPacksPacket{}.write(emptyPacks);
    CHECK(SelectKnownPacksPacket::read(emptyPacks)->packs.empty());

    nbt::NbtCompound biome;
    biome.putBoolean("has_precipitation", true);
    biome.putFloat("temperature", 0.8F);
    const RegistryDataPacket registry{
        "minecraft:worldgen/biome",
        {{"minecraft:plains", nbt::NbtTag::ofCompound(std::move(biome))}, {"minecraft:the_void", std::nullopt}}};
    io::ByteBuffer registryBuffer;
    registry.write(registryBuffer);
    const auto readRegistry = RegistryDataPacket::read(registryBuffer);
    REQUIRE(readRegistry.has_value());
    CHECK(*readRegistry == registry);

    const TagsPacket tags{{{"minecraft:block", {{"minecraft:logs", {1, 2, 3}}, {"minecraft:planks", {}}}}}};
    io::ByteBuffer tagsBuffer;
    tags.write(tagsBuffer);
    CHECK(*TagsPacket::read(tagsBuffer) == tags);

    const BrandPacket brand{"Basalt"};
    io::ByteBuffer brandBuffer;
    brand.write(brandBuffer);
    CHECK(*BrandPacket::read(brandBuffer) == brand);
    // A payload on another channel is not a brand packet, whatever it carries.
    io::ByteBuffer otherChannel;
    writeString(otherChannel, "basalt:something");
    writeString(otherChannel, "x");
    CHECK_FALSE(BrandPacket::read(otherChannel).has_value());
}

TEST_CASE("the login plugin packets round trip, payload and all") {
    using namespace basalt::wire::protocol::login;

    const LoginPluginRequestPacket request{7, "velocity:player_info", {1, 2, 3, 4}};
    io::ByteBuffer requestBuffer;
    request.write(requestBuffer);
    CHECK(*LoginPluginRequestPacket::read(requestBuffer) == request);

    const LoginPluginResponsePacket answered{7, std::vector<std::uint8_t>{9, 9}};
    io::ByteBuffer answeredBuffer;
    answered.write(answeredBuffer);
    CHECK(*LoginPluginResponsePacket::read(answeredBuffer) == answered);

    // What every real client answers: it does not know the channel.
    const LoginPluginResponsePacket unknown{7, std::nullopt};
    io::ByteBuffer unknownBuffer;
    unknown.write(unknownBuffer);
    CHECK(*LoginPluginResponsePacket::read(unknownBuffer) == unknown);
}

TEST_CASE("the simple play packets round trip") {
    using namespace basalt::wire::protocol::play;

    const CooldownPacket cooldown{"minecraft:ender_pearl", 20};
    io::ByteBuffer cooldownBuffer;
    cooldown.write(cooldownBuffer);
    CHECK(*CooldownPacket::read(cooldownBuffer) == cooldown);

    // The payload runs to the end of the frame, with no length of its own.
    const CustomPayloadPacket payload{"basalt:hello", {1, 2, 3, 4, 5}};
    io::ByteBuffer payloadBuffer;
    payload.write(payloadBuffer);
    CHECK(*CustomPayloadPacket::read(payloadBuffer) == payload);

    const AwardStatsPacket stats{{{AwardStatsPacket::kTypeMined, 1, 42}, {AwardStatsPacket::kTypeCustom, 8, 7}}};
    io::ByteBuffer statsBuffer;
    stats.write(statsBuffer);
    CHECK(*AwardStatsPacket::read(statsBuffer) == stats);

    const ChunksBiomesPacket biomes{{{5, -7, {1, 2, 3}}, {0, 0, {}}}};
    io::ByteBuffer biomesBuffer;
    biomes.write(biomesBuffer);
    CHECK(*ChunksBiomesPacket::read(biomesBuffer) == biomes);
    // Negative chunk coordinates have to survive the packing, which is where sign errors hide.
    CHECK(ChunkBiomeData::packChunkPos(-1, -1) == -1);

    ExplodePacket explosion;
    explosion.x = 10.5;
    explosion.y = 64.0;
    explosion.z = -3.25;
    explosion.radius = 4.0F;
    explosion.blockCount = 17;
    explosion.particleId = 3;
    explosion.sound = "minecraft:entity.generic.explode";
    explosion.blockParticles = {{1, ExplodePacket::kPoofScaling, 0.0F, 1}, {2, ExplodePacket::kSmokeScaling, 0.0F, 1}};
    io::ByteBuffer explosionBuffer;
    explosion.write(explosionBuffer);
    CHECK(*ExplodePacket::read(explosionBuffer) == explosion);

    // An explosion that pushed somebody carries three more doubles, and one that did not carries
    // none: the flag is what tells them apart, and both shapes have to read back.
    explosion.knockbackX = 0.5;
    explosion.knockbackY = 1.0;
    explosion.knockbackZ = -0.25;
    io::ByteBuffer pushedBuffer;
    explosion.write(pushedBuffer);
    CHECK(*ExplodePacket::read(pushedBuffer) == explosion);
}

TEST_CASE("the world border packets round trip") {
    using namespace basalt::wire::protocol::play;

    const InitializeBorderPacket initialize{
        0.0, 0.0, 60000000.0, 60000000.0, 0, InitializeBorderPacket::kAbsoluteMaxSize, 5, 15};
    io::ByteBuffer initializeBuffer;
    initialize.write(initializeBuffer);
    CHECK(*InitializeBorderPacket::read(initializeBuffer) == initialize);

    const SetBorderLerpSizePacket lerp{100.0, 50.0, 12000};
    io::ByteBuffer lerpBuffer;
    lerp.write(lerpBuffer);
    CHECK(*SetBorderLerpSizePacket::read(lerpBuffer) == lerp);

    const TransferPacket transfer{"other.basaltmc.org", 25565};
    io::ByteBuffer transferBuffer;
    transfer.write(transferBuffer);
    CHECK(*TransferPacket::read(transferBuffer) == transfer);

    const TickingStatePacket ticking{20.0F, true};
    io::ByteBuffer tickingBuffer;
    ticking.write(tickingBuffer);
    CHECK(*TickingStatePacket::read(tickingBuffer) == ticking);

    // A truncated frame is a failed read, not a packet full of zeroes taken as real.
    io::ByteBuffer truncated;
    truncated.writeByte(1);
    CHECK_FALSE(InitializeBorderPacket::read(truncated).has_value());
}

TEST_CASE("the HUD and sound packets round trip") {
    using namespace basalt::wire::protocol::play;

    const TitleTextPacket title{text::Component::text("Welcome", text::kGold).bold()};
    io::ByteBuffer titleBuffer;
    title.write(titleBuffer);
    CHECK(*TitleTextPacket::read(titleBuffer) == title);

    const SetTitlesAnimationPacket animation{10, 70, 20};
    io::ByteBuffer animationBuffer;
    animation.write(animationBuffer);
    CHECK(*SetTitlesAnimationPacket::read(animationBuffer) == animation);

    const SoundPacket sound{
        "minecraft:block.note_block.harp", SoundSource::Records, 10.5, 64.0, -3.125, 1.0F, 1.5F, 123456789};
    io::ByteBuffer soundBuffer;
    sound.write(soundBuffer);
    const auto readSound = SoundPacket::read(soundBuffer);
    REQUIRE(readSound.has_value());
    // Positions travel in eighths of a block, so they come back quantised, not exact.
    CHECK(readSound->sound == sound.sound);
    CHECK(readSound->source == SoundSource::Records);
    CHECK(readSound->x == 10.5);
    CHECK(readSound->z == doctest::Approx(-3.125));
    CHECK(readSound->seed == sound.seed);

    // The command names are not always the constant's own: `record`, `block` and `player`.
    CHECK(soundSourceCommandName(SoundSource::Records) == "record");
    CHECK(soundSourceCommandName(SoundSource::Blocks) == "block");
    CHECK(soundSourceCommandName(SoundSource::Players) == "player");
    CHECK(soundSourceByCommandName("record") == SoundSource::Records);
    CHECK_FALSE(soundSourceByCommandName("records").has_value());

    // All four shapes of the stop-sound flag byte.
    for (const StopSoundPacket& stop :
         std::vector<StopSoundPacket>{{std::nullopt, std::nullopt},
                                      {std::nullopt, SoundSource::Music},
                                      {std::string{"minecraft:music.game"}, std::nullopt},
                                      {std::string{"minecraft:music.game"}, SoundSource::Music}}) {
        io::ByteBuffer stopBuffer;
        stop.write(stopBuffer);
        CHECK(*StopSoundPacket::read(stopBuffer) == stop);
    }
}

TEST_CASE("the command suggestion packets round trip") {
    using namespace basalt::wire::protocol::play;

    const CommandSuggestionRequestPacket request{3, "/give @p minecraft:st"};
    io::ByteBuffer requestBuffer;
    request.write(requestBuffer);
    CHECK(*CommandSuggestionRequestPacket::read(requestBuffer) == request);

    const CommandSuggestionsPacket suggestions{
        3, 9, 11, {{"minecraft:stone", text::Component::text("Stone")}, {"minecraft:stick", std::nullopt}}};
    io::ByteBuffer suggestionsBuffer;
    suggestions.write(suggestionsBuffer);
    CHECK(*CommandSuggestionsPacket::read(suggestionsBuffer) == suggestions);
}

TEST_CASE("the player state packets round trip") {
    using namespace basalt::wire::protocol::play;

    const SetHealthPacket health{15.5F, 18, 3.25F};
    io::ByteBuffer healthBuffer;
    health.write(healthBuffer);
    CHECK(*SetHealthPacket::read(healthBuffer) == health);

    const SetExperiencePacket experience{0.75F, 30, 1395};
    io::ByteBuffer experienceBuffer;
    experience.write(experienceBuffer);
    CHECK(*SetExperiencePacket::read(experienceBuffer) == experience);

    const ClientboundPlayerAbilitiesPacket abilities{true, true, true, true, 0.1F, 0.2F};
    io::ByteBuffer abilitiesBuffer;
    abilities.write(abilitiesBuffer);
    CHECK(*ClientboundPlayerAbilitiesPacket::read(abilitiesBuffer) == abilities);
    // The serverbound twin sends the same byte but only the flying bit survives the round trip.
    io::ByteBuffer toggle;
    ServerboundPlayerAbilitiesPacket{true}.write(toggle);
    CHECK(ServerboundPlayerAbilitiesPacket::read(toggle)->flying);

    SpawnInfo info;
    info.dimension = 0;
    info.dimensionName = "minecraft:overworld";
    info.hashedSeed = 1234567890123LL;
    info.gamemode = Gamemode::Creative;
    info.previousGamemode = -1;
    info.death = GlobalPos{"minecraft:the_nether", BlockPos{10, 64, -20}};
    info.seaLevel = 63;
    const RespawnPacket respawn{info, RespawnPacket::kKeepAllData};
    io::ByteBuffer respawnBuffer;
    respawn.write(respawnBuffer);
    CHECK(*RespawnPacket::read(respawnBuffer) == respawn);

    // A death point in another dimension is what puts the compass needle back in the overworld, so
    // both the present and absent shapes have to read back.
    info.death.reset();
    io::ByteBuffer aliveBuffer;
    RespawnPacket{info, 0}.write(aliveBuffer);
    CHECK_FALSE(RespawnPacket::read(aliveBuffer)->spawnInfo.death.has_value());

    const PlayerCommandPacket command{42, PlayerCommandPacket::Action::StartSprinting, 0};
    io::ByteBuffer commandBuffer;
    command.write(commandBuffer);
    CHECK(*PlayerCommandPacket::read(commandBuffer) == command);
    // An ordinal past the end of the enum is refused, not cast into whatever it lands on.
    io::ByteBuffer badAction;
    varInt::write(badAction, 42);
    varInt::write(badAction, 99);
    varInt::write(badAction, 0);
    CHECK_FALSE(PlayerCommandPacket::read(badAction).has_value());

    const PlayerCombatKillPacket death{7, text::Component::text("Steve fell out of the world")};
    io::ByteBuffer deathBuffer;
    death.write(deathBuffer);
    CHECK(*PlayerCombatKillPacket::read(deathBuffer) == death);

    const RemoveEntitiesPacket remove{{1, 2, 3, 4}};
    io::ByteBuffer removeBuffer;
    remove.write(removeBuffer);
    CHECK(*RemoveEntitiesPacket::read(removeBuffer) == remove);
}

TEST_CASE("the block entity packets round trip") {
    using namespace basalt::wire::protocol::play;

    nbt::NbtCompound sign;
    sign.putString("front_text", "hello");
    const BlockEntityDataPacket data{BlockPos{1, 2, 3}, 7, sign};
    io::ByteBuffer dataBuffer;
    data.write(dataBuffer);
    CHECK(*BlockEntityDataPacket::read(dataBuffer) == data);

    // An absent tag goes out as TAG_End, one byte, and comes back absent.
    const BlockEntityDataPacket cleared{BlockPos{1, 2, 3}, 7, std::nullopt};
    io::ByteBuffer clearedBuffer;
    cleared.write(clearedBuffer);
    CHECK(*BlockEntityDataPacket::read(clearedBuffer) == cleared);

    const BlockEventPacket event{BlockPos{-4, 70, 9}, 1, 2, 54};
    io::ByteBuffer eventBuffer;
    event.write(eventBuffer);
    CHECK(*BlockEventPacket::read(eventBuffer) == event);

    const SignUpdatePacket update{BlockPos{0, 64, 0}, true, {"one", "two", "", "four"}};
    io::ByteBuffer updateBuffer;
    update.write(updateBuffer);
    CHECK(*SignUpdatePacket::read(updateBuffer) == update);

    // The three flags are one byte, so a packet with two of them set and one clear pins the bits.
    const SetCommandBlockPacket programmed{
        BlockPos{3, -60, 8}, "say hello", SetCommandBlockPacket::Mode::Sequence, true, false, true};
    io::ByteBuffer programmedBuffer;
    programmed.write(programmedBuffer);
    CHECK(*SetCommandBlockPacket::read(programmedBuffer) == programmed);

    // A negative offset byte and the flag byte with two of its four bits set pin both encodings.
    const SetStructureBlockPacket structure{.pos = BlockPos{-4, 70, 12},
                                            .update = SetStructureBlockPacket::Update::SaveArea,
                                            .mode = SetStructureBlockPacket::Mode::Save,
                                            .name = "minecraft:house",
                                            .offset = {-3, 1, 48},
                                            .size = {5, 6, 7},
                                            .mirror = 2,
                                            .rotation = 3,
                                            .data = "chest",
                                            .ignoreEntities = false,
                                            .strict = true,
                                            .showAir = true,
                                            .showBoundingBox = false,
                                            .integrity = 0.5F,
                                            .seed = -123456789012LL};
    io::ByteBuffer structureBuffer;
    structure.write(structureBuffer);
    CHECK(*SetStructureBlockPacket::read(structureBuffer) == structure);
}

TEST_CASE("item components round trip, including the ones this build has no codec for") {
    using namespace basalt::wire::protocol::play;

    const auto roundTrip = [](const ItemComponent& component, std::optional<std::size_t> length = std::nullopt) {
        io::ByteBuffer buffer;
        component.writePayload(buffer);
        const std::size_t written = buffer.readableBytes();
        auto read = ItemComponent::readPayload(buffer, component.id(), length ? length : std::optional{written});
        REQUIRE(read.has_value());
        CHECK(*read == component);
    };

    roundTrip(ItemComponent{dataComponent::kDamage, ItemComponent::Number{42}});
    roundTrip(ItemComponent{dataComponent::kDyedColor, ItemComponent::Color{0xFF8800}});
    roundTrip(ItemComponent{dataComponent::kUnbreakable, ItemComponent::Unit{}});
    roundTrip(ItemComponent{dataComponent::kEnchantmentGlintOverride, ItemComponent::Flag{true}});
    roundTrip(ItemComponent{dataComponent::kCustomName, ItemComponent::Text{text::Component::text("Excalibur")}});
    roundTrip(ItemComponent{dataComponent::kLore,
                            ItemComponent::Lore{{text::Component::text("a"), text::Component::text("b")}}});
    roundTrip(ItemComponent{dataComponent::kEnchantments, ItemComponent::Enchantments{{{3, 5}, {7, 1}}, false}});
    roundTrip(ItemComponent{dataComponent::kStoredEnchantments, ItemComponent::Enchantments{{{3, 5}}, true}});

    nbt::NbtCompound custom;
    custom.putInt("plugin_value", 7);
    roundTrip(ItemComponent{dataComponent::kCustomData, ItemComponent::CustomData{custom}});

    // An unknown component keeps its bytes, so a stack that goes out to a client and comes back is
    // the same stack. Without a length there is nothing to keep, and it says so.
    const ItemComponent unknown{999, ItemComponent::Raw{{1, 2, 3}}};
    roundTrip(unknown, std::size_t{3});
    io::ByteBuffer buffer;
    unknown.writePayload(buffer);
    CHECK_FALSE(ItemComponent::readPayload(buffer, 999, std::nullopt).has_value());
}

TEST_CASE("particle options round trip through every shape") {
    using namespace basalt::wire::protocol::play;

    const auto roundTrip = [](const ParticleOptions& options, ParticleOptionShape shape) {
        io::ByteBuffer buffer;
        options.write(buffer);
        const auto read = ParticleOptions::read(buffer, shape);
        REQUIRE(read.has_value());
        CHECK(*read == options);
        CHECK(buffer.readableBytes() == 0);
    };

    roundTrip(ParticleOptions{ParticleOptions::BlockState{1234}}, ParticleOptionShape::BlockState);
    roundTrip(ParticleOptions{ParticleOptions::Color{0x00FF00}}, ParticleOptionShape::Color);
    roundTrip(ParticleOptions{ParticleOptions::Spell{0xFF0000, 0.5F}}, ParticleOptionShape::Spell);
    roundTrip(ParticleOptions{ParticleOptions::Dust{0x0000FF, 1.5F}}, ParticleOptionShape::Dust);
    roundTrip(ParticleOptions{ParticleOptions::DustTransition{1, 2, 0.75F}}, ParticleOptionShape::DustTransition);
    roundTrip(ParticleOptions{ParticleOptions::Trail{1.0, 2.0, 3.0, 0xABCDEF, 40}}, ParticleOptionShape::Trail);
    roundTrip(ParticleOptions{ParticleOptions::Number{2.5F}}, ParticleOptionShape::Number);
    roundTrip(ParticleOptions{ParticleOptions::Delay{7}}, ParticleOptionShape::Delay);
    roundTrip(ParticleOptions{ParticleOptions::Geyser{3}}, ParticleOptionShape::Geyser);
    roundTrip(ParticleOptions{ParticleOptions::GeyserBase{3, 0.25F}}, ParticleOptionShape::GeyserBase);
    roundTrip(ParticleOptions{ParticleOptions::Vibration{BlockPos{1, -2, 3}, 20}}, ParticleOptionShape::Vibration);

    ParticleOptions::Item item;
    item.itemId = 5;
    item.count = 2;
    item.components.emplace_back(dataComponent::kDamage, ItemComponent::Number{3});
    roundTrip(ParticleOptions{item}, ParticleOptionShape::Item);

    io::ByteBuffer empty;
    CHECK_FALSE(ParticleOptions::read(empty, ParticleOptionShape::None).has_value());
}

TEST_CASE("the recipe book packets round trip") {
    using namespace basalt::wire::protocol::play;

    RecipeDisplay shaped;
    shaped.grid = RecipeDisplay::Grid{2, 2};
    shaped.ingredients = {
        SlotDisplay{SlotDisplay::Item{1}}, SlotDisplay{SlotDisplay::Empty{}},
        SlotDisplay{SlotDisplay::Composite{{SlotDisplay{SlotDisplay::Item{2}}, SlotDisplay{SlotDisplay::Item{3}}}}},
        SlotDisplay{SlotDisplay::Item{4}}};
    shaped.result = SlotDisplay{SlotDisplay::Item{9}};
    shaped.craftingStation = SlotDisplay{SlotDisplay::Empty{}};

    RecipeBookEntry entry;
    entry.recipeId = 11;
    entry.display = shaped;
    entry.craftingRequirements = {{1, 2}, {3}};
    entry.highlight = true;

    const RecipeBookAddPacket add{{entry}, true};
    io::ByteBuffer addBuffer;
    add.write(addBuffer);
    const auto readAdd = RecipeBookAddPacket::read(addBuffer);
    REQUIRE(readAdd.has_value());
    CHECK(*readAdd == add);
    CHECK(addBuffer.readableBytes() == 0);

    // A shapeless recipe has no grid, so it writes two fewer varints than a shaped one.
    RecipeBookEntry shapeless = entry;
    shapeless.display.grid.reset();
    shapeless.craftingRequirements.clear();
    io::ByteBuffer shapelessBuffer;
    RecipeBookAddPacket{{shapeless}, false}.write(shapelessBuffer);
    CHECK(RecipeBookAddPacket::read(shapelessBuffer)->entries.front() == shapeless);

    const RecipeBookRemovePacket remove{{1, 2, 3}};
    io::ByteBuffer removeBuffer;
    remove.write(removeBuffer);
    CHECK(*RecipeBookRemovePacket::read(removeBuffer) == remove);
}

TEST_CASE("the property sets and the stonecutter list round trip") {
    using namespace basalt::wire::protocol::play;

    const UpdateRecipesPacket recipes{
        .propertySets = {{"minecraft:brewing_reagent", {7, 8}}, {"minecraft:smithing_base", {}}},
        .stonecutter = {{{1, 2, 3}, 40, 1}, {{4}, 41, 4}}};
    io::ByteBuffer buffer;
    recipes.write(buffer);
    const auto read = UpdateRecipesPacket::read(buffer);
    REQUIRE(read.has_value());
    CHECK(*read == recipes);
    CHECK(buffer.readableBytes() == 0);
}

TEST_CASE("the command tree round trips with its argument parsers") {
    using namespace basalt::wire::protocol::play;
    using Parser = ArgumentParser;

    // A tree shaped the way a real one is: a root, a literal under it, and an argument under that.
    CommandNodeData root;
    root.kind = CommandNodeData::Kind::Root;
    root.children = {1};

    CommandNodeData literal;
    literal.kind = CommandNodeData::Kind::Literal;
    literal.name = "give";
    literal.children = {2};

    CommandNodeData argument;
    argument.kind = CommandNodeData::Kind::Argument;
    argument.name = "targets";
    argument.executable = true;
    argument.parser = Parser{argumentType::kEntity, Parser::EntityProperties{false, true}};
    argument.suggestionId = "minecraft:ask_server";
    argument.redirect = 0;

    const CommandsPacket packet{{root, literal, argument}, 0};
    io::ByteBuffer buffer;
    packet.write(buffer);
    const auto read = CommandsPacket::read(buffer);
    REQUIRE(read.has_value());
    CHECK(*read == packet);
    CHECK(buffer.readableBytes() == 0);
}

TEST_CASE("every shape of argument parser properties round trips") {
    using namespace basalt::wire::protocol::play;
    using Parser = ArgumentParser;

    const auto roundTrip = [](const Parser& parser) {
        io::ByteBuffer buffer;
        parser.write(buffer);
        const auto read = Parser::read(buffer);
        REQUIRE(read.has_value());
        CHECK(*read == parser);
        CHECK(buffer.readableBytes() == 0);
    };

    // A singleton parser carries nothing at all after its id.
    roundTrip(Parser{argumentType::kBrigadierBool});
    roundTrip(Parser{argumentType::kUuid});

    roundTrip(Parser{argumentType::kBrigadierString, Parser::StringProperties{Parser::StringKind::GreedyPhrase}});
    roundTrip(Parser{argumentType::kEntity, Parser::EntityProperties{true, false}});
    roundTrip(Parser{argumentType::kScoreHolder, Parser::ScoreHolderProperties{true}});
    roundTrip(Parser{argumentType::kTime, Parser::TimeProperties{20}});
    roundTrip(Parser{argumentType::kResource, Parser::RegistryProperties{"minecraft:item"}});
    roundTrip(Parser{argumentType::kResourceOrTagKey, Parser::RegistryProperties{"minecraft:block"}});

    // A bound that is absent is absent on the wire too, so all four combinations have to survive
    // and an unbounded parser must not come back carrying a sentinel.
    roundTrip(Parser{argumentType::kBrigadierInteger, Parser::Bounds<std::int32_t>{}});
    roundTrip(Parser{argumentType::kBrigadierInteger, Parser::Bounds<std::int32_t>{0, std::nullopt}});
    roundTrip(Parser{argumentType::kBrigadierInteger, Parser::Bounds<std::int32_t>{std::nullopt, 64}});
    roundTrip(Parser{argumentType::kBrigadierInteger, Parser::Bounds<std::int32_t>{-1, 1}});
    roundTrip(Parser{argumentType::kBrigadierDouble, Parser::Bounds<double>{0.5, 2.5}});
    roundTrip(Parser{argumentType::kBrigadierFloat, Parser::Bounds<float>{std::nullopt, 1.0F}});

    // An id past the end of the registry is refused, not read as whatever it lands on.
    io::ByteBuffer unknown;
    varInt::write(unknown, argumentType::kCount);
    CHECK_FALSE(Parser::read(unknown).has_value());
}

TEST_CASE("a typed command round trips") {
    using namespace basalt::wire::protocol::play;
    const ChatCommandPacket command{"give @p minecraft:stone 64"};
    io::ByteBuffer buffer;
    command.write(buffer);
    CHECK(*ChatCommandPacket::read(buffer) == command);
}

TEST_CASE("item stacks round trip in both of their wire forms") {
    using namespace basalt::wire::protocol::play;

    WireItemStack sword;
    sword.itemId = 900;
    sword.count = 1;
    sword.components.emplace_back(dataComponent::kDamage, ItemComponent::Number{12});
    sword.components.emplace_back(dataComponent::kEnchantments, ItemComponent::Enchantments{{{3, 4}}, false});
    sword.removedComponents = {dataComponent::kMaxDamage};

    io::ByteBuffer buffer;
    sword.write(buffer);
    const auto read = WireItemStack::read(buffer);
    REQUIRE(read.has_value());
    CHECK(*read == sword);
    CHECK(read->damage() == 12);
    CHECK(read->enchantments() == std::vector<std::pair<std::int32_t, std::int32_t>>{{3, 4}});

    // An empty slot is a single zero, and comes back as nothing at all.
    io::ByteBuffer empty;
    WireItemStack::writeOptional(empty, std::nullopt);
    CHECK(empty.readableBytes() == 1);
    CHECK_FALSE(WireItemStack::read(empty).has_value());

    // The delimited form keeps a component it cannot name, because a creative client echoes slots
    // it merely looked at and dropping the unknown ones once stripped enchanted items.
    io::ByteBuffer delimited;
    varInt::write(delimited, 1);
    varInt::write(delimited, 900);
    varInt::write(delimited, 1);
    varInt::write(delimited, 0);
    varInt::write(delimited, 999);
    varInt::write(delimited, 3);
    delimited.writeBytes(std::vector<std::uint8_t>{7, 7, 7});
    const auto kept = WireItemStack::readDelimited(delimited);
    REQUIRE(kept.has_value());
    REQUIRE(kept->components.size() == 1);
    CHECK(kept->components.front().id() == 999);
    CHECK(kept->components.front().as<ItemComponent::Raw>()->payload == std::vector<std::uint8_t>{7, 7, 7});
    CHECK(delimited.readableBytes() == 0);
}

TEST_CASE("the container packets round trip") {
    using namespace basalt::wire::protocol::play;

    WireItemStack stone;
    stone.itemId = 1;
    stone.count = 64;

    const ContainerSetContentPacket content{
        ContainerSetContentPacket::kPlayerInventory, 7, {stone, std::nullopt, stone}, std::nullopt};
    io::ByteBuffer contentBuffer;
    content.write(contentBuffer);
    CHECK(*ContainerSetContentPacket::read(contentBuffer) == content);

    const ContainerSetSlotPacket slot{0, 7, 36, stone};
    io::ByteBuffer slotBuffer;
    slot.write(slotBuffer);
    CHECK(*ContainerSetSlotPacket::read(slotBuffer) == slot);

    const OpenScreenPacket screen{1, menuType::kGeneric9x3, text::Component::text("Chest")};
    io::ByteBuffer screenBuffer;
    screen.write(screenBuffer);
    CHECK(*OpenScreenPacket::read(screenBuffer) == screen);

    const ContainerSetDataPacket furnace{1, ContainerSetDataPacket::kFurnaceCookingProgress, 42};
    io::ByteBuffer furnaceBuffer;
    furnace.write(furnaceBuffer);
    CHECK(*ContainerSetDataPacket::read(furnaceBuffer) == furnace);

    const ContainerButtonClickPacket button{1, ContainerButtonClickPacket::kLecternNextPage};
    io::ByteBuffer buttonBuffer;
    button.write(buttonBuffer);
    CHECK(*ContainerButtonClickPacket::read(buttonBuffer) == button);

    // The slot comes **before** the window in this one, which is the other way round from the click.
    const ContainerSlotStateChangedPacket state{4, 7, true};
    io::ByteBuffer stateBuffer;
    state.write(stateBuffer);
    CHECK(*ContainerSlotStateChangedPacket::read(stateBuffer) == state);
}

TEST_CASE("a container click skips its hashed prediction without losing the stream") {
    using namespace basalt::wire::protocol::play;

    io::ByteBuffer buffer;
    varInt::write(buffer, 1);
    varInt::write(buffer, 7);
    buffer.writeShort(36);
    buffer.writeByte(0);
    varInt::write(buffer, static_cast<std::int32_t>(ServerboundContainerClickPacket::Mode::QuickMove));

    // One changed slot whose predicted stack carries a component hash and a removal.
    varInt::write(buffer, 1);
    buffer.writeShort(36);
    writeBoolean(buffer, true);
    varInt::write(buffer, 1);
    varInt::write(buffer, 64);
    varInt::write(buffer, 1);
    varInt::write(buffer, dataComponent::kDamage);
    buffer.writeInt(0x12345678);
    varInt::write(buffer, 1);
    varInt::write(buffer, dataComponent::kMaxDamage);

    // The carried item: present, and empty of components.
    writeBoolean(buffer, true);
    varInt::write(buffer, 2);
    varInt::write(buffer, 1);
    varInt::write(buffer, 0);
    varInt::write(buffer, 0);

    // A trailing byte that must survive, which is what proves the skipping landed on the right spot.
    buffer.writeByte(0xAB);

    const auto click = ServerboundContainerClickPacket::read(buffer);
    REQUIRE(click.has_value());
    CHECK(click->slot == 36);
    CHECK(click->mode == ServerboundContainerClickPacket::Mode::QuickMove);
    CHECK(click->clientHasCarriedItem);
    CHECK(buffer.readableBytes() == 1);
    CHECK(buffer.readByte() == 0xAB);
}

TEST_CASE("merchant offers round trip") {
    using namespace basalt::wire::protocol::play;

    WireItemStack emerald;
    emerald.itemId = 800;
    emerald.count = 1;

    MerchantOffer offer;
    offer.costA = MerchantCost{800, 3};
    offer.result = emerald;
    offer.costB = MerchantCost{801, 1};
    offer.maxUses = 12;
    offer.experience = 2;
    offer.priceMultiplier = 0.05F;

    const MerchantOffersPacket packet{1, {offer}, 2, 30, true, true};
    io::ByteBuffer buffer;
    packet.write(buffer);
    const auto read = MerchantOffersPacket::read(buffer);
    REQUIRE(read.has_value());
    CHECK(*read == packet);
    CHECK(buffer.readableBytes() == 0);
}

TEST_CASE("the scoreboard packets round trip") {
    using namespace basalt::wire::protocol::play;

    // The blank format is what removes the red number, which is the whole reason a sidebar is
    // usable, so it has to survive as itself and not fold into "no format at all".
    SetObjectivePacket objective;
    objective.objectiveName = "sidebar";
    objective.method = SetObjectivePacket::kMethodAdd;
    objective.displayName = text::Component::text("Basalt", text::kGold);
    objective.numberFormat = NumberFormat{NumberFormat::Blank{}};
    io::ByteBuffer objectiveBuffer;
    objective.write(objectiveBuffer);
    CHECK(*SetObjectivePacket::read(objectiveBuffer) == objective);

    // A remove writes nothing past the method, and reading one back must not go looking for more.
    SetObjectivePacket removal;
    removal.objectiveName = "sidebar";
    removal.method = SetObjectivePacket::kMethodRemove;
    io::ByteBuffer removalBuffer;
    removal.write(removalBuffer);
    CHECK(removalBuffer.readableBytes() == 1 + 7 + 1);
    CHECK(*SetObjectivePacket::read(removalBuffer) == removal);

    const SetDisplayObjectivePacket display{DisplaySlot::Sidebar, "sidebar"};
    io::ByteBuffer displayBuffer;
    display.write(displayBuffer);
    CHECK(*SetDisplayObjectivePacket::read(displayBuffer) == display);

    SetScorePacket score;
    score.owner = "line_1";
    score.objectiveName = "sidebar";
    score.score = 5;
    score.display = text::Component::text("Players online: 3");
    score.numberFormat = NumberFormat{NumberFormat::Fixed{text::Component::text("*")}};
    io::ByteBuffer scoreBuffer;
    score.write(scoreBuffer);
    CHECK(*SetScorePacket::read(scoreBuffer) == score);

    const ResetScorePacket reset{"line_1", std::string{"sidebar"}};
    io::ByteBuffer resetBuffer;
    reset.write(resetBuffer);
    CHECK(*ResetScorePacket::read(resetBuffer) == reset);
    io::ByteBuffer everywhere;
    ResetScorePacket{"line_1", std::nullopt}.write(everywhere);
    CHECK_FALSE(ResetScorePacket::read(everywhere)->objectiveName.has_value());
}

TEST_CASE("every boss bar operation round trips") {
    using namespace basalt::wire::protocol::play;

    const common::Uuid id{1, 2};
    const std::vector<BossEventPacket::Operation> operations = {
        BossEventPacket::Add{text::Component::text("The Ender Dragon"), 0.75F, BossBarColor::Purple,
                             BossBarOverlay::Notched10, true, true, false},
        BossEventPacket::Remove{},
        BossEventPacket::UpdateProgress{0.5F},
        BossEventPacket::UpdateName{text::Component::text("Almost there")},
        BossEventPacket::UpdateStyle{BossBarColor::Red, BossBarOverlay::Notched20},
        BossEventPacket::UpdateProperties{false, true, true},
    };

    for (const BossEventPacket::Operation& operation : operations) {
        const BossEventPacket packet{id, operation};
        io::ByteBuffer buffer;
        packet.write(buffer);
        const auto read = BossEventPacket::read(buffer);
        REQUIRE(read.has_value());
        CHECK(*read == packet);
        CHECK(buffer.readableBytes() == 0);
    }

    CHECK(bossBarOverlayName(BossBarOverlay::Notched6) == "notched_6");
}

TEST_CASE("the team packets round trip on every method") {
    using namespace basalt::wire::protocol::play;

    SetPlayerTeamPacket::Parameters parameters;
    parameters.displayName = text::Component::text("Red");
    parameters.prefix = text::Component::text("[R] ");
    parameters.nameTagVisibility = NameTagVisibility::HideForOtherTeams;
    parameters.collisionRule = CollisionRule::PushOwnTeam;
    parameters.color = TeamColor::Red;
    parameters.friendlyFire = false;

    const SetPlayerTeamPacket add{"red", SetPlayerTeamPacket::kMethodAdd, parameters, {"Steve", "Alex"}};
    io::ByteBuffer addBuffer;
    add.write(addBuffer);
    CHECK(*SetPlayerTeamPacket::read(addBuffer) == add);

    // A remove carries neither parameters nor members, and reading one must not look for either.
    const SetPlayerTeamPacket remove{"red", SetPlayerTeamPacket::kMethodRemove, std::nullopt, {}};
    io::ByteBuffer removeBuffer;
    remove.write(removeBuffer);
    CHECK(*SetPlayerTeamPacket::read(removeBuffer) == remove);

    const SetPlayerTeamPacket join{"red", SetPlayerTeamPacket::kMethodJoin, std::nullopt, {"Notch"}};
    io::ByteBuffer joinBuffer;
    join.write(joinBuffer);
    CHECK(*SetPlayerTeamPacket::read(joinBuffer) == join);

    CHECK(nameTagVisibilityName(NameTagVisibility::HideForOtherTeams) == "hideForOtherTeams");
    CHECK(collisionRuleByName("pushOwnTeam") == CollisionRule::PushOwnTeam);
    CHECK(teamColorByName("dark_aqua") == TeamColor::DarkAqua);
    CHECK(teamColorRgb(TeamColor::Gold) == 0xFFAA00);
}

TEST_CASE("the tab list and dialog packets round trip") {
    using namespace basalt::wire::protocol::play;

    const TabListPacket list{text::Component::text("Welcome"), text::Component::text("basaltmc.org")};
    io::ByteBuffer listBuffer;
    list.write(listBuffer);
    CHECK(*TabListPacket::read(listBuffer) == list);

    // The dialog id carries a plus one on the wire, because zero means an inline dialog.
    const ShowDialogPacket dialog{7};
    io::ByteBuffer dialogBuffer;
    dialog.write(dialogBuffer);
    CHECK(dialogBuffer.peekByte(0) == 8);
    CHECK(*ShowDialogPacket::read(dialogBuffer) == dialog);
}

TEST_CASE("entity metadata round trips every serializer this build writes") {
    using namespace basalt::wire::protocol::play;
    using Data = SetEntityDataPacket;

    WireItemStack stack;
    stack.itemId = 1;
    stack.count = 3;

    Data packet;
    packet.entityId = 42;
    packet.entries = {
        {Data::kSharedFlagsIndex, Data::ByteValue{Data::kFlagSneaking | Data::kFlagOnFire}},
        {Data::kAirSupplyIndex, Data::IntValue{200}},
        {Data::kPlayerAbsorptionIndex, Data::FloatValue{4.0F}},
        {20, Data::TextValue{text::Component::text("Display")}},
        {Data::kCustomNameIndex, Data::OptionalTextValue{text::Component::text("Bob")}},
        {Data::kCustomNameVisibleIndex, Data::BoolValue{true}},
        {Data::kItemStackIndex, Data::ItemValue{stack}},
        {21, Data::RotationsValue{1.0F, 2.0F, 3.0F}},
        {22, Data::VillagerDataValue{1, 2, 3}},
        {Data::kSleepingPosIndex, Data::OptionalBlockPosValue{BlockPos{1, 2, 3}}},
        {Data::kPoseIndex, Data::PoseValue{Pose::Sleeping}},
        {23, Data::RegistryIdValue{5, RegistryHolder::WolfVariant}},
        {15, Data::RegistryIdValue{0, RegistryHolder::HumanoidArm}},
        {24, Data::RegistryIdValue{14, RegistryHolder::DyeColor}},
    };

    io::ByteBuffer buffer;
    packet.write(buffer);
    const auto read = Data::read(buffer);
    REQUIRE(read.has_value());
    CHECK(*read == packet);
    CHECK(buffer.readableBytes() == 0);

    // An absent optional is not the same as never having sent one, so it has to survive as absent.
    Data cleared;
    cleared.entityId = 42;
    cleared.entries = {{Data::kCustomNameIndex, Data::OptionalTextValue{std::nullopt}},
                       {Data::kSleepingPosIndex, Data::OptionalBlockPosValue{std::nullopt}}};
    io::ByteBuffer clearedBuffer;
    cleared.write(clearedBuffer);
    CHECK(*Data::read(clearedBuffer) == cleared);

    // A serializer this build cannot name has no width, so it is refused rather than skipped.
    io::ByteBuffer unknown;
    varInt::write(unknown, 42);
    unknown.writeByte(0);
    varInt::write(unknown, 99);
    CHECK_FALSE(Data::read(unknown).has_value());

    // parity: a dragon's breath cloud, `dragon_breath` with its power.
    Data breath;
    breath.entityId = 7;
    breath.entries.push_back(
        {.index = 10,
         .value = Data::ParticleValue{.particleId = 9, .options = ParticleOptions{ParticleOptions::Number{1.0F}}}});
    io::ByteBuffer breathBuffer;
    breath.write(breathBuffer);
    const Data::ParticleShapeOf shapes = [](std::int32_t) { return ParticleOptionShape::Number; };
    CHECK(Data::read(breathBuffer, shapes) == breath);
    io::ByteBuffer blind;
    breath.write(blind);
    CHECK_FALSE(Data::read(blind).has_value());

    // parity: a living entity's effect swirl, one coloured `entity_effect` per visible effect.
    Data swirl;
    swirl.entityId = 8;
    swirl.entries.push_back(
        {.index = 10,
         .value = Data::ParticlesValue{
             {Data::ParticleValue{.particleId = 20, .options = ParticleOptions{ParticleOptions::Color{-16776961}}},
              Data::ParticleValue{.particleId = 21}}}});
    io::ByteBuffer swirlBuffer;
    swirl.write(swirlBuffer);
    const Data::ParticleShapeOf colours = [](std::int32_t id) {
        return id == 20 ? ParticleOptionShape::Color : ParticleOptionShape::None;
    };
    CHECK(Data::read(swirlBuffer, colours) == swirl);

    // parity: a firework stuck to whoever used it names them one above their id, and nobody as zero.
    Data rocket;
    rocket.entityId = 9;
    rocket.entries.push_back({.index = 9, .value = Data::OptionalUnsignedIntValue{41}});
    rocket.entries.push_back({.index = 9, .value = Data::OptionalUnsignedIntValue{}});
    rocket.entries.push_back({.index = 18, .value = Data::LongValue{-1}});
    rocket.entries.push_back({.index = 18, .value = Data::LongValue{24000123456LL}});
    io::ByteBuffer rocketBuffer;
    rocket.write(rocketBuffer);
    CHECK(Data::read(rocketBuffer) == rocket);
}

TEST_CASE("the entity metadata helpers write the indices vanilla expects") {
    using namespace basalt::wire::protocol::play;
    using Data = SetEntityDataPacket;

    // parity: 26.2 put Avatar between LivingEntity and Player, so absorption is 17 and skin parts
    // 16. An ageable mob's own fields start after that, and a wrong index crashes the client.
    CHECK(Data::playerAbsorption(1, 4.0F).entries.front().index == 17);
    CHECK(Data::skinParts(1, 0x7F).entries.front().index == 16);

    const Data sleeping = Data::sleeping(1, BlockPos{0, 64, 0});
    REQUIRE(sleeping.entries.size() == 2);
    CHECK(std::get<Data::PoseValue>(sleeping.entries[0].value).pose == Pose::Sleeping);
    CHECK(sleeping.entries[1].index == Data::kSleepingPosIndex);
    CHECK(std::get<Data::PoseValue>(Data::sleeping(1, std::nullopt).entries[0].value).pose == Pose::Standing);

    // The flags byte carries all of them at once, which is why the helper takes the whole byte:
    // writing only the one a caller knows about erases the rest.
    const Data submersion = Data::submersion(1, 300, Data::kFlagOnFire | Data::kFlagSprinting);
    CHECK(std::get<Data::ByteValue>(submersion.entries[0].value).value ==
          static_cast<std::int8_t>(Data::kFlagOnFire | Data::kFlagSprinting));
}

TEST_CASE("spawning an entity round trips its position and rotations") {
    using namespace basalt::wire::protocol::play;

    SpawnEntityPacket packet;
    packet.entityId = 7;
    packet.uuid = common::Uuid{1, 2};
    packet.entityType = SpawnEntityPacket::kPlayerEntityType;
    packet.x = 10.5;
    packet.y = 64.0;
    packet.z = -3.25;
    packet.velocityX = 0.5;
    packet.yaw = 90.0F;
    packet.pitch = -45.0F;

    io::ByteBuffer buffer;
    packet.write(buffer);
    const auto read = SpawnEntityPacket::read(buffer);
    REQUIRE(read.has_value());
    CHECK(read->entityId == packet.entityId);
    CHECK(read->uuid == packet.uuid);
    CHECK(read->x == packet.x);
    // Rotations travel as one byte, so they come back quantised to 256 steps per turn.
    CHECK(read->yaw == doctest::Approx(90.0F).epsilon(0.01));
    CHECK(read->pitch == doctest::Approx(-45.0F).epsilon(0.01));
    CHECK(read->velocityX == doctest::Approx(0.5).epsilon(1e-3));
    CHECK(buffer.readableBytes() == 0);
}

TEST_CASE("the entity packets round trip") {
    using namespace basalt::wire::protocol::play;

    WireItemStack helmet;
    helmet.itemId = 300;
    helmet.count = 1;

    // The continuation bit is set on every slot but the last, which is what makes several slots fit
    // in one packet with no count in front of them.
    const SetEquipmentPacket equipment{
        7, {{SetEquipmentPacket::SlotType::Head, helmet}, {SetEquipmentPacket::SlotType::MainHand, std::nullopt}}};
    io::ByteBuffer equipmentBuffer;
    equipment.write(equipmentBuffer);
    CHECK(*SetEquipmentPacket::read(equipmentBuffer) == equipment);

    const LevelEventPacket levelEvent{LevelEventPacket::kParticlesDestroyBlock, BlockPos{1, 2, 3}, 15, false};
    io::ByteBuffer levelEventBuffer;
    levelEvent.write(levelEventBuffer);
    CHECK(*LevelEventPacket::read(levelEventBuffer) == levelEvent);

    const BlockDestructionPacket cracks{7, BlockPos{1, 2, 3}, 5};
    io::ByteBuffer cracksBuffer;
    cracks.write(cracksBuffer);
    CHECK(*BlockDestructionPacket::read(cracksBuffer) == cracks);

    // Both link ids are plain ints, not varints: a varint here writes a shorter frame.
    io::ByteBuffer linkBuffer;
    SetEntityLinkPacket{7, SetEntityLinkPacket::kNoVehicle}.write(linkBuffer);
    CHECK(linkBuffer.readableBytes() == 8);
    CHECK(SetEntityLinkPacket::read(linkBuffer)->destinationId == -1);

    const UpdateMobEffectPacket effect{7, 3, 1, 600, false, true, true, false};
    io::ByteBuffer effectBuffer;
    effect.write(effectBuffer);
    CHECK(*UpdateMobEffectPacket::read(effectBuffer) == effect);

    const EntityPositionSyncPacket sync{7, 1.5, 64.0, -2.5, 90.0F, 0.0F, true};
    io::ByteBuffer syncBuffer;
    sync.write(syncBuffer);
    CHECK(*EntityPositionSyncPacket::read(syncBuffer) == sync);

    const SwingAnimationPacket swing{7, 1, SwingAnimation{SwingAnimationType::Stab, 19}};
    io::ByteBuffer swingBuffer;
    swing.write(swingBuffer);
    CHECK(*SwingAnimationPacket::read(swingBuffer) == swing);

    const SetPassengersPacket passengers{7, {8, 9}};
    io::ByteBuffer passengersBuffer;
    passengers.write(passengersBuffer);
    CHECK(*SetPassengersPacket::read(passengersBuffer) == passengers);

    // A permission level is an entity event in the twenties, and the client needs it before its own
    // parser will accept a selector.
    CHECK(EntityEventPacket::permissionLevel(7, 2).eventId == 26);
    CHECK(EntityEventPacket::permissionLevel(7, 99).eventId == 28);
}

TEST_CASE("a damage event carries its optional source position") {
    using namespace basalt::wire::protocol::play;

    DamageEventPacket damage;
    damage.entityId = 7;
    damage.damageType = 3;
    damage.causeEntityId = 8;
    damage.directEntityId = -1;

    io::ByteBuffer buffer;
    damage.write(buffer);
    const auto read = DamageEventPacket::read(buffer);
    REQUIRE(read.has_value());
    CHECK(*read == damage);
    // A hit nobody dealt is -1 in, and -1 back out, because the wire carries it as zero.
    CHECK(read->directEntityId == -1);

    damage.sourceX = 1.0;
    damage.sourceY = 2.0;
    damage.sourceZ = 3.0;
    io::ByteBuffer positioned;
    damage.write(positioned);
    CHECK(*DamageEventPacket::read(positioned) == damage);
}

TEST_CASE("attributes and their modifiers round trip") {
    using namespace basalt::wire::protocol::play;

    UpdateAttributesPacket packet;
    packet.entityId = 7;
    packet.attributes = {
        {1,
         0.1,
         {{common::Identifier{"minecraft", "sprinting"}, 0.3, UpdateAttributesPacket::Operation::AddMultipliedTotal}}},
        {2, 20.0, {}}};

    io::ByteBuffer buffer;
    packet.write(buffer);
    const auto read = UpdateAttributesPacket::read(buffer);
    REQUIRE(read.has_value());
    CHECK(*read == packet);
    CHECK(buffer.readableBytes() == 0);
}

TEST_CASE("a tracked waypoint writes a three-byte colour, not four") {
    using namespace basalt::wire::protocol::play;

    TrackedWaypointPacket waypoint;
    waypoint.operation = TrackedWaypointPacket::Operation::Track;
    waypoint.entityUuid = common::Uuid{1, 2};
    waypoint.style = "minecraft:default";
    waypoint.color = 0xFF8800;
    waypoint.x = 100;
    waypoint.y = 64;
    waypoint.z = -100;

    io::ByteBuffer buffer;
    waypoint.write(buffer);
    const auto read = TrackedWaypointPacket::read(buffer);
    REQUIRE(read.has_value());
    CHECK(*read == waypoint);
    CHECK(buffer.readableBytes() == 0);

    // Without a colour the three bytes are simply absent.
    waypoint.color.reset();
    io::ByteBuffer plain;
    waypoint.write(plain);
    CHECK(*TrackedWaypointPacket::read(plain) == waypoint);
}

TEST_CASE("particles carry their options only when the type has them") {
    using namespace basalt::wire::protocol::play;

    LevelParticlesPacket plain;
    plain.particleTypeId = 20;
    plain.x = 1.0;
    plain.count = 5;
    io::ByteBuffer plainBuffer;
    plain.write(plainBuffer);
    CHECK(*LevelParticlesPacket::read(plainBuffer, ParticleOptionShape::None) == plain);

    LevelParticlesPacket spread = plain;
    spread.maxSpeedY = 0.5F;
    spread.randomization = LevelParticlesPacket::Randomization::AlternativeWithSpeed;
    io::ByteBuffer spreadBuffer;
    spread.write(spreadBuffer);
    CHECK(*LevelParticlesPacket::read(spreadBuffer, ParticleOptionShape::None) == spread);

    LevelParticlesPacket dust = plain;
    dust.options = ParticleOptions{ParticleOptions::Dust{0xFF0000, 1.0F}};
    io::ByteBuffer dustBuffer;
    dust.write(dustBuffer);
    CHECK(*LevelParticlesPacket::read(dustBuffer, ParticleOptionShape::Dust) == dust);
}

TEST_CASE("the world packets round trip") {
    using namespace basalt::wire::protocol::play;

    LightData light;
    light.skyLightMask = {0x3};
    light.blockLightMask = {0x1};
    // An empty mask is no words at all, the only shape java.util.BitSet reads back.
    light.emptySkyLightMask = {};
    light.emptyBlockLightMask = {0x4};
    light.skyLight = {std::vector<std::uint8_t>(2048, 0xFF), std::vector<std::uint8_t>(2048, 0x11)};
    light.blockLight = {std::vector<std::uint8_t>(2048, 0x00)};

    nbt::NbtCompound sign;
    sign.putString("text", "hi");

    MapChunkPacket chunk;
    chunk.x = 3;
    chunk.z = -4;
    chunk.heightmaps = {{HeightmapType::MotionBlocking, {1, 2, 3}}};
    chunk.chunkData = {1, 2, 3, 4, 5};
    chunk.blockEntities = {{0x21, 64, 7, sign}, {0x00, -60, 9, std::nullopt}};
    chunk.light = light;

    io::ByteBuffer chunkBuffer;
    chunk.write(chunkBuffer);
    const auto readChunk = MapChunkPacket::read(chunkBuffer);
    REQUIRE(readChunk.has_value());
    CHECK(*readChunk == chunk);
    CHECK(chunkBuffer.readableBytes() == 0);

    const UpdateLightPacket lightPacket{3, -4, light};
    io::ByteBuffer lightBuffer;
    lightPacket.write(lightBuffer);
    CHECK(*UpdateLightPacket::read(lightBuffer) == lightPacket);

    // Negative chunk coordinates have to survive the packing, which is where sign errors hide.
    const ForgetLevelChunkPacket forget{-1, -1};
    io::ByteBuffer forgetBuffer;
    forget.write(forgetBuffer);
    CHECK(*ForgetLevelChunkPacket::read(forgetBuffer) == forget);

    const SetTimePacket time{123456, {{SetTimePacket::kOverworldClock, 6000, 0.5F, 1.0F}}};
    io::ByteBuffer timeBuffer;
    time.write(timeBuffer);
    CHECK(*SetTimePacket::read(timeBuffer) == time);

    const ClientboundBlockUpdatePacket block{BlockPos{-1, 64, -1}, 1234};
    io::ByteBuffer blockBuffer;
    block.write(blockBuffer);
    CHECK(*ClientboundBlockUpdatePacket::read(blockBuffer) == block);

    const AddTransientBlockPacket transient{BlockPos{5, -3, 9}, 4321};
    io::ByteBuffer transientBuffer;
    transient.write(transientBuffer);
    CHECK(*AddTransientBlockPacket::read(transientBuffer) == transient);
}

TEST_CASE("a section update packs its coordinates and its changes") {
    using namespace basalt::wire::protocol::play;

    SectionBlocksUpdatePacket packet;
    packet.sectionX = -2;
    packet.sectionY = -4;
    packet.sectionZ = 3;
    packet.changes = {{SectionBlocksUpdatePacket::packPosition(1, 2, 3), 100},
                      {SectionBlocksUpdatePacket::packPosition(15, 15, 15), 200}};

    io::ByteBuffer buffer;
    packet.write(buffer);
    const auto read = SectionBlocksUpdatePacket::read(buffer);
    REQUIRE(read.has_value());
    CHECK(*read == packet);
    CHECK(buffer.readableBytes() == 0);

    // parity: SectionPos.sectionRelativePos is x, then z, then y, which is not the order the
    // arguments come in.
    CHECK(SectionBlocksUpdatePacket::packPosition(1, 2, 3) == 0x132);
}

TEST_CASE("the join and position packets round trip") {
    using namespace basalt::wire::protocol::play;

    SpawnInfo spawnInfo;
    spawnInfo.dimensionName = "minecraft:overworld";
    spawnInfo.gamemode = Gamemode::Creative;

    LoginPlayPacket login;
    login.entityId = 1;
    login.worldNames = {"minecraft:overworld", "minecraft:the_nether"};
    login.spawnInfo = spawnInfo;
    login.viewDistance = 12;

    io::ByteBuffer loginBuffer;
    login.write(loginBuffer);
    CHECK(*LoginPlayPacket::read(loginBuffer) == login);

    const PlayerPositionPacket position{7, 0.5, 64.0, 0.5, 0.0, 0.0, 0.0, 90.0F, 0.0F, 0};
    io::ByteBuffer positionBuffer;
    position.write(positionBuffer);
    CHECK(*PlayerPositionPacket::read(positionBuffer) == position);

    const EntityTeleportPacket teleport{2, 1.0, 2.0, 3.0, 0.0, 0.0, 0.0, 45.0F, -10.0F, 0, true};
    io::ByteBuffer teleportBuffer;
    teleport.write(teleportBuffer);
    CHECK(*EntityTeleportPacket::read(teleportBuffer) == teleport);

    // The movement packets carry the on-ground flag as a bitfield, not a boolean.
    const ServerboundPositionLookPacket move{1.0, 64.0, 2.0, 180.0F, 0.0F, true};
    io::ByteBuffer moveBuffer;
    move.write(moveBuffer);
    CHECK(*ServerboundPositionLookPacket::read(moveBuffer) == move);
}

TEST_CASE("the tab list packets round trip and refuse what they cannot parse") {
    using namespace basalt::wire::protocol::play;

    PlayerInfoEntry entry;
    entry.uuid = common::Uuid::offlinePlayer("Steve");
    entry.name = "Steve";
    entry.listed = true;
    entry.latencyMillis = 42;
    entry.gamemode = Gamemode::Creative;
    entry.properties.push_back({"textures", "base64", std::string{"signature"}});

    const auto initializing = PlayerInfoUpdatePacket::initializing({entry});
    io::ByteBuffer buffer;
    initializing.write(buffer);
    const auto read = PlayerInfoUpdatePacket::read(buffer);
    REQUIRE(read.has_value());
    CHECK(*read == initializing);

    const auto gameMode = PlayerInfoUpdatePacket::gameMode(entry.uuid, Gamemode::Spectator);
    io::ByteBuffer gameModeBuffer;
    gameMode.write(gameModeBuffer);
    CHECK(*PlayerInfoUpdatePacket::read(gameModeBuffer) == gameMode);

    const auto session = PlayerInfoUpdatePacket::chatSession(
        entry.uuid, RemoteChatSessionData{.sessionId = common::Uuid{3, 4}, .expiresAt = 5, .publicKey = {6}});
    io::ByteBuffer sessionBuffer;
    session.write(sessionBuffer);
    CHECK(*PlayerInfoUpdatePacket::read(sessionBuffer) == session);

    // An action whose fields have unknown width cannot be walked past, so the packet is refused.
    io::ByteBuffer unsupported;
    unsupported.writeByte(PlayerInfoUpdatePacket::kUpdateDisplayName);
    varInt::write(unsupported, 0);
    CHECK_FALSE(PlayerInfoUpdatePacket::read(unsupported).has_value());

    const PlayerInfoRemovePacket remove{{entry.uuid}};
    io::ByteBuffer removeBuffer;
    remove.write(removeBuffer);
    CHECK(*PlayerInfoRemovePacket::read(removeBuffer) == remove);
}

TEST_CASE("the block interaction packets round trip") {
    using namespace basalt::wire::protocol::play;

    const ServerboundPlayerActionPacket action{ServerboundPlayerActionPacket::Action::StopDestroyBlock,
                                               BlockPos{1, 2, 3}, Direction::Up, 12};
    io::ByteBuffer actionBuffer;
    action.write(actionBuffer);
    CHECK(*ServerboundPlayerActionPacket::read(actionBuffer) == action);

    const ServerboundUseItemOnPacket use{
        Hand::MainHand, BlockHitResult{BlockPos{1, 2, 3}, Direction::North, 0.5F, 0.5F, 0.0F, false, false}, 13};
    io::ByteBuffer useBuffer;
    use.write(useBuffer);
    CHECK(*ServerboundUseItemOnPacket::read(useBuffer) == use);

    const ServerboundUseItemPacket bucket{Hand::OffHand, 14, 90.0F, 0.0F};
    io::ByteBuffer bucketBuffer;
    bucket.write(bucketBuffer);
    CHECK(*ServerboundUseItemPacket::read(bucketBuffer) == bucket);

    const AcceptTeleportationPacket accepted{3, 1.5, 70.0, -2.5, 180.0F, 10.0F};
    io::ByteBuffer acceptedBuffer;
    accepted.write(acceptedBuffer);
    CHECK(*AcceptTeleportationPacket::read(acceptedBuffer) == accepted);

    const PickItemFromBlockPacket pick{BlockPos{1, 2, 3}, true};
    io::ByteBuffer pickBuffer;
    pick.write(pickBuffer);
    CHECK(*PickItemFromBlockPacket::read(pickBuffer) == pick);

    // An action ordinal past the end of the enum is refused, not cast into whatever it lands on.
    io::ByteBuffer bad;
    varInt::write(bad, 99);
    bad.writeLong(0);
    bad.writeByte(0);
    varInt::write(bad, 0);
    CHECK_FALSE(ServerboundPlayerActionPacket::read(bad).has_value());
}

TEST_CASE("the chat packets round trip") {
    using namespace basalt::wire::protocol::play;

    const SystemChatPacket system{text::Component::text("Server restarting", text::kRed), false};
    io::ByteBuffer systemBuffer;
    system.write(systemBuffer);
    CHECK(*SystemChatPacket::read(systemBuffer) == system);

    const auto roundTrips = [](const auto& packet) {
        io::ByteBuffer buffer;
        packet.write(buffer);
        const auto read = std::remove_cvref_t<decltype(packet)>::read(buffer);
        REQUIRE(read.has_value());
        CHECK(*read == packet);
        CHECK(buffer.readableBytes() == 0);
    };
    MessageSignature signature{};
    signature.fill(0x7F);

    ChatMessagePacket chat;
    chat.message = "hello";
    chat.timestamp = 1234;
    chat.salt = 5678;
    chat.lastSeen.offset = 2;
    chat.lastSeen.setAcknowledged(0);
    chat.lastSeen.setAcknowledged(19);
    chat.lastSeen.checksum = -3;
    roundTrips(chat);
    chat.signature = signature;
    roundTrips(chat);
    CHECK(chat.lastSeen.bitLength() == 20);

    roundTrips(ChatAckPacket{7});
    roundTrips(SignedChatCommandPacket{.command = "msg Steve hi",
                                       .timestamp = 99,
                                       .salt = -1,
                                       .argumentSignatures = {{"message", signature}},
                                       .lastSeen = {}});
    roundTrips(ChatSessionUpdatePacket{
        {.sessionId = common::Uuid{1, 2}, .expiresAt = 1700000000000, .publicKey = {1, 2, 3}, .keySignature = {4, 5}}});

    PlayerChatPacket player;
    player.globalIndex = 3;
    player.sender = common::Uuid{5, 6};
    player.index = 1;
    player.signature = signature;
    player.content = "hi";
    player.lastSeen = {PackedSignature{.id = 4, .full = std::nullopt},
                       PackedSignature{.id = PackedSignature::kFullSignature, .full = signature}};
    player.unsignedContent = text::Component::text("hi!");
    player.filterMask = FilterMask{.type = FilterMask::Type::PartiallyFiltered, .mask = {0b101}};
    player.chatType = ChatTypeBound{
        .chatType = 0, .name = text::Component::text("Steve"), .targetName = text::Component::text("Alex")};
    roundTrips(player);
    roundTrips(DisguisedChatPacket{.message = text::Component::text("hey"),
                                   .chatType = {.chatType = 4, .name = text::Component::text("Server")}});

    // parity: the holder id is written one up, since zero means an inline chat type.
    io::ByteBuffer bound;
    ChatTypeBound{.chatType = 4, .name = text::Component::text("x")}.write(bound);
    CHECK(bound.readByte() == 5);
}

TEST_CASE("the map item data packet round trips with and without its optional parts") {
    using namespace basalt::wire::protocol::play;

    MapItemDataPacket bare{.mapId = 7, .scale = 2, .locked = true};
    io::ByteBuffer bareBuffer;
    bare.write(bareBuffer);
    // parity: id, scale, locked, no decorations, zero width.
    CHECK(bareBuffer.toVector() == std::vector<std::uint8_t>{7, 2, 1, 0, 0});
    CHECK(*MapItemDataPacket::read(bareBuffer) == bare);

    MapItemDataPacket full{.mapId = 300, .scale = 0, .locked = false};
    full.decorations = std::vector<MapDecorationEntry>{
        {.type = 0, .x = -128, .y = 127, .rotation = 15},
        {.type = 10, .x = 3, .y = -4, .rotation = 8, .name = text::Component::text("Base")}};
    full.patch = MapColorPatch{.startX = 1, .startY = 2, .width = 2, .height = 1, .colors = {0x7C, 0x2A}};
    io::ByteBuffer fullBuffer;
    full.write(fullBuffer);
    CHECK(*MapItemDataPacket::read(fullBuffer) == full);
}

TEST_CASE("the vehicle packets round-trip") {
    using namespace basalt::wire::protocol::play;

    const ServerboundMoveVehiclePacket moved{
        .movingTo = {.x = 1.5, .y = 64.0, .z = -3.25, .yRot = 90.0F, .xRot = -10.0F}, .onGround = true};
    io::ByteBuffer movedBuffer;
    moved.write(movedBuffer);
    // parity: three doubles, two floats and a boolean.
    CHECK(movedBuffer.readableBytes() == 33);
    CHECK(*ServerboundMoveVehiclePacket::read(movedBuffer) == moved);

    const ServerboundPaddleBoatPacket paddles{.left = true, .right = false};
    io::ByteBuffer paddlesBuffer;
    paddles.write(paddlesBuffer);
    CHECK(*ServerboundPaddleBoatPacket::read(paddlesBuffer) == paddles);

    const ClientboundMoveVehiclePacket back{.movingTo = {.x = 2.0, .y = 70.0, .z = 4.0, .yRot = 0.0F, .xRot = 0.0F}};
    io::ByteBuffer backBuffer;
    back.write(backBuffer);
    CHECK(*ClientboundMoveVehiclePacket::read(backBuffer) == back);

    const MountScreenOpenPacket screen{.containerId = 3, .inventoryColumns = 5, .entityId = 42};
    io::ByteBuffer screenBuffer;
    screen.write(screenBuffer);
    CHECK(*MountScreenOpenPacket::read(screenBuffer) == screen);

    // Rotations are packed bytes, so the test uses angles a byte holds exactly.
    const ClientboundMoveMinecartPacket steps{.entityId = 7,
                                              .steps = {MinecartStep{.position = {1.0, 2.5, 3.0},
                                                                     .movement = {0.1, 0.0, -0.2},
                                                                     .yRot = 90.0F,
                                                                     .xRot = -45.0F,
                                                                     .weight = 0.75F}}};
    io::ByteBuffer stepsBuffer;
    steps.write(stepsBuffer);
    // parity: a varint id and count, then six doubles, two bytes and a float per step.
    CHECK(stepsBuffer.readableBytes() == 2 + 48 + 2 + 4);
    CHECK(*ClientboundMoveMinecartPacket::read(stepsBuffer) == steps);

    const ServerboundSetCommandMinecartPacket command{.entityId = 9, .command = "say hi", .trackOutput = true};
    io::ByteBuffer commandBuffer;
    command.write(commandBuffer);
    CHECK(*ServerboundSetCommandMinecartPacket::read(commandBuffer) == command);
}

TEST_CASE("the enabled features round-trip") {
    using basalt::wire::protocol::configuration::UpdateEnabledFeaturesPacket;
    const UpdateEnabledFeaturesPacket features{.features = {"minecraft:vanilla", "minecraft:minecart_improvements"}};
    io::ByteBuffer buffer;
    features.write(buffer);
    CHECK(*UpdateEnabledFeaturesPacket::read(buffer) == features);
}

TEST_CASE("the advancement packets round-trip, positions after the telemetry flag") {
    using namespace basalt::wire::protocol::play;
    UpdateAdvancementsPacket update;
    update.reset = true;
    update.added.push_back(AdvancementEntry{
        .id = "minecraft:story/root",
        .parent = std::nullopt,
        .display =
            AdvancementDisplayData{
                .title = text::Component::translatable("advancements.story.root.title"),
                .description = text::Component::text("d"),
                .icon = WireItemStack{.itemId = 5, .count = 1, .components = {}, .removedComponents = {}},
                .frame = 2,
                .background = "minecraft:gui/advancements/backgrounds/stone",
                .showToast = false,
                .hidden = true},
        .requirements = {{"a", "b"}, {"c"}},
        .sendsTelemetryEvent = true,
        .x = 1.0F,
        .y = 2.5F});
    update.added.push_back(AdvancementEntry{.id = "minecraft:story/child", .parent = "minecraft:story/root"});
    update.removed = {"minecraft:gone"};
    update.progress.push_back(
        AdvancementProgressData{.id = "minecraft:story/root", .criteria = {{"a", 1234}, {"b", std::nullopt}}});
    update.showAdvancements = false;
    io::ByteBuffer buffer;
    update.write(buffer);
    CHECK(UpdateAdvancementsPacket::read(buffer) == update);

    io::ByteBuffer tab;
    SelectAdvancementsTabPacket{.tab = "minecraft:story/root"}.write(tab);
    CHECK(SelectAdvancementsTabPacket::read(tab)->tab == "minecraft:story/root");

    io::ByteBuffer seen;
    varInt::write(seen, 0);
    writeString(seen, "minecraft:adventure/root");
    const auto opened = SeenAdvancementsPacket::read(seen);
    REQUIRE(opened.has_value());
    CHECK(opened->action == SeenAdvancementsPacket::Action::OpenedTab);
    CHECK(opened->tab == "minecraft:adventure/root");
}

TEST_CASE("frame writers reject empty and oversized raw packets without changing queued frames") {
    io::ByteBuffer output;
    output.writeByte(0x42);
    const std::vector<std::uint8_t> tooLarge(static_cast<std::size_t>(packetFraming::kMaxFrameSize) + 1, 1);
    const auto raw = packetFraming::writeUncompressed(output, tooLarge);
    REQUIRE_FALSE(raw);
    CHECK(raw.error() == FramingError::FrameTooBig);
    CHECK(output.toVector() == std::vector<std::uint8_t>{0x42});
    const auto belowThreshold = packetFraming::writeCompressed(output, tooLarge, packetFraming::kMaxUncompressedSize);
    REQUIRE_FALSE(belowThreshold);
    CHECK(belowThreshold.error() == FramingError::FrameTooBig);
    CHECK(output.toVector() == std::vector<std::uint8_t>{0x42});
    CHECK_FALSE(packetFraming::writeUncompressed(output, {}));
    CHECK_FALSE(packetFraming::writeCompressed(output, {}, 0));
    CHECK(output.toVector() == std::vector<std::uint8_t>{0x42});
}

TEST_CASE("frame inspection bounds declarations before body buffering or inflation") {
    using packetFraming::inspectFrame;
    const std::array<std::uint8_t, 1> partial{0x80};
    auto absent = inspectFrame(partial, false);
    REQUIRE(absent);
    CHECK_FALSE(*absent);
    const std::array<std::uint8_t, 2> raw{0x81, 0x08};
    auto hint = inspectFrame(raw, false);
    REQUIRE(hint);
    REQUIRE(*hint);
    CHECK((**hint).frameBytes == 1027);
    CHECK((**hint).packetBytes == 1025);
    CHECK_FALSE((**hint).complete);
    CHECK(inspectFrame(raw, false, 1024).error() == FramingError::FrameTooBig);
    const std::array<std::uint8_t, 4> expanded{4, 0x80, 0x80, 0x40};
    CHECK(inspectFrame(expanded, true, 1024).error() == FramingError::FrameTooBig);
    const std::array<std::uint8_t, 4> borrowing{1, 0x80, 1, 0};
    CHECK(inspectFrame(borrowing, true).error() == FramingError::MalformedVarInt);
    const std::array<std::uint8_t, 2> empty{1, 0};
    CHECK(inspectFrame(empty, true).error() == FramingError::EmptyFrame);
    const std::array<std::uint8_t, 2> noCompressedBody{1, 8};
    CHECK(inspectFrame(noCompressedBody, true).error() == FramingError::EmptyFrame);
    const std::array<std::uint8_t, 3> below{2, 8, 0};
    CHECK(inspectFrame(below, true, 1024, 256).error() == FramingError::BelowCompressionThreshold);
    const std::array<std::uint8_t, 2> awaitingData{3, 0x80};
    auto incomplete = inspectFrame(awaitingData, true);
    REQUIRE(incomplete);
    REQUIRE(*incomplete);
    CHECK_FALSE((**incomplete).packetBytes);
    CHECK_FALSE((**incomplete).complete);
}
