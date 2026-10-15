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

TEST_CASE("a malformed VarInt fails the buffer instead of throwing") {
    io::ByteBuffer tooLong = io::ByteBuffer::wrap(std::vector<std::uint8_t>{0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x01});
    static_cast<void>(varInt::read(tooLong));
    CHECK(tooLong.failed());

    io::ByteBuffer truncated = io::ByteBuffer::wrap(std::vector<std::uint8_t>{0x80});
    static_cast<void>(varInt::read(truncated));
    CHECK(truncated.failed());
}

TEST_CASE("a bit set travels as java.util.BitSet.toByteArray") {
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
    // parity: UUID.nameUUIDFromBytes("OfflinePlayer:<name>"), the id every offline server hands.
    CHECK(common::Uuid::offlinePlayer("Steve").toString() == "5627dd98-e6be-3c21-b8a8-e92344183641");
    CHECK(common::Uuid::offlinePlayer("Alex").toString() == "36532b5e-c442-3dbb-a24c-c7e55d0f979a");
    CHECK(common::Uuid::offlinePlayer("Notch").toString() == "b50ad385-829d-3141-a216-7e7d7539ba7f");
    CHECK(common::Uuid::parse("5627dd98-e6be-3c21-b8a8-e92344183641") == common::Uuid::offlinePlayer("Steve"));
    CHECK(common::Uuid::offlinePlayer("Steve").toUndashedString() == "5627dd98e6be3c21b8a8e92344183641");
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

TEST_CASE("the simple play packets round trip") {
    using namespace basalt::wire::protocol::play;

    const CooldownPacket cooldown{"minecraft:ender_pearl", 20};
    io::ByteBuffer cooldownBuffer;
    cooldown.write(cooldownBuffer);
    CHECK(*CooldownPacket::read(cooldownBuffer) == cooldown);

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

    explosion.knockbackX = 0.5;
    explosion.knockbackY = 1.0;
    explosion.knockbackZ = -0.25;
    io::ByteBuffer pushedBuffer;
    explosion.write(pushedBuffer);
    CHECK(*ExplodePacket::read(pushedBuffer) == explosion);
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

    // An unknown component keeps its bytes, so a stack that goes out to a client and comes back is the same stack.
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

TEST_CASE("the command tree round trips with its argument parsers") {
    using namespace basalt::wire::protocol::play;
    using Parser = ArgumentParser;

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

    io::ByteBuffer empty;
    WireItemStack::writeOptional(empty, std::nullopt);
    CHECK(empty.readableBytes() == 1);
    CHECK_FALSE(WireItemStack::read(empty).has_value());

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

TEST_CASE("a container click skips its hashed prediction without losing the stream") {
    using namespace basalt::wire::protocol::play;

    io::ByteBuffer buffer;
    varInt::write(buffer, 1);
    varInt::write(buffer, 7);
    buffer.writeShort(36);
    buffer.writeByte(0);
    varInt::write(buffer, static_cast<std::int32_t>(ServerboundContainerClickPacket::Mode::QuickMove));

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

TEST_CASE("the entity packets round trip") {
    using namespace basalt::wire::protocol::play;

    WireItemStack helmet;
    helmet.itemId = 300;
    helmet.count = 1;

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

    CHECK(EntityEventPacket::permissionLevel(7, 2).eventId == 26);
    CHECK(EntityEventPacket::permissionLevel(7, 99).eventId == 28);
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
