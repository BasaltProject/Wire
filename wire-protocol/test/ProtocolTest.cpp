#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <basalt/wire/common/Base64.hpp>
#include <basalt/wire/protocol/PacketFraming.hpp>
#include <basalt/wire/protocol/ProtocolBuffer.hpp>
#include <basalt/wire/protocol/VarInt.hpp>
#include <basalt/wire/protocol/configuration/ConfigurationPackets.hpp>
#include <basalt/wire/protocol/handshake/HandshakePacket.hpp>
#include <basalt/wire/protocol/login/LoginPackets.hpp>
#include <basalt/wire/protocol/login/LoginPluginPackets.hpp>
#include <basalt/wire/protocol/play/BlockEntityPackets.hpp>
#include <basalt/wire/protocol/play/BlockPos.hpp>
#include <basalt/wire/protocol/play/BorderPackets.hpp>
#include <basalt/wire/protocol/play/HudPackets.hpp>
#include <basalt/wire/protocol/play/ItemComponent.hpp>
#include <basalt/wire/protocol/play/LowPrecisionVec3.hpp>
#include <basalt/wire/protocol/play/NibbleArray.hpp>
#include <basalt/wire/protocol/play/PalettedContainer.hpp>
#include <basalt/wire/protocol/play/ParticleOptions.hpp>
#include <basalt/wire/protocol/play/PlayerStatePackets.hpp>
#include <basalt/wire/protocol/play/SimplePackets.hpp>
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

TEST_CASE("the configuration packets round trip") {
    using namespace basalt::wire::protocol::configuration;

    const ClientInformationPacket information{"en_us", 12, 0, true, 0x7F, 1, false, true, 0};
    io::ByteBuffer informationBuffer;
    information.write(informationBuffer);
    CHECK(*ClientInformationPacket::read(informationBuffer) == information);

    const SelectKnownPacksPacket packs{{{"minecraft", "core", "26.2"}}};
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
    info.previousGamemode = 255;
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
