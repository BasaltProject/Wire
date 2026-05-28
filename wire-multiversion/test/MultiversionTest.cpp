#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <basalt/wire/multiversion/ChunkRewriter.hpp>
#include <basalt/wire/multiversion/CompactBitStorage.hpp>
#include <basalt/wire/multiversion/ComposedTables.hpp>
#include <basalt/wire/multiversion/IdMap.hpp>
#include <basalt/wire/multiversion/ProtocolChain.hpp>
#include <basalt/wire/multiversion/ProtocolGate.hpp>
#include <basalt/wire/multiversion/ProtocolVersions.hpp>
#include <basalt/wire/multiversion/java/EntityDataRewriter.hpp>
#include <basalt/wire/multiversion/java/JavaTranslators.hpp>
#include <basalt/wire/multiversion/java/LoginFields.hpp>
#include <basalt/wire/multiversion/java/LongMaskLight.hpp>
#include <basalt/wire/multiversion/java/Tables775.hpp>
#include <basalt/wire/nbt/NbtTag.hpp>
#include <basalt/wire/protocol/ProtocolBuffer.hpp>
#include <basalt/wire/protocol/configuration/ConfigurationPackets.hpp>
#include <basalt/wire/protocol/play/AdvancementPackets.hpp>
#include <basalt/wire/protocol/play/EntityPackets.hpp>
#include <basalt/wire/protocol/play/InventoryPackets.hpp>
#include <basalt/wire/protocol/play/MapPackets.hpp>
#include <basalt/wire/protocol/play/PalettedContainer.hpp>
#include <basalt/wire/protocol/play/PlayerStatePackets.hpp>
#include <basalt/wire/protocol/play/WorldPackets.hpp>

#include <doctest.h>

#include <cstdint>
#include <map>
#include <vector>

using namespace basalt::wire::multiversion;

TEST_CASE("an id map reads runs of constant difference") {
    const IdMap map{"0:10:0;20:10:-2|100>7"};

    CHECK(map.map(0) == 0);
    CHECK(map.map(9) == 9);
    CHECK(map.map(20) == 18);
    CHECK(map.map(29) == 27);
    CHECK(map.map(100) == 7);
    CHECK(map.substitutes(100));
    CHECK_FALSE(map.substitutes(20));
    CHECK(map.map(50) == IdLookup::kNone);

    CHECK(map.unmap(18) == 20);
    CHECK(map.unmap(0) == 0);
    CHECK(map.unmap(1000) == IdLookup::kNone);

    CHECK(IdMap::identity().map(12345) == 12345);
    CHECK(IdMap::identity().unmap(12345) == 12345);
}

TEST_CASE("the pre-1.16 packing spills entries across word boundaries") {
    // That straddle is exactly what 1.16 stopped doing and what every link below it has to write back.
    std::vector<std::int32_t> values(40);
    for (std::size_t i = 0; i < values.size(); ++i) {
        values[i] = static_cast<std::int32_t>(i % 32);
    }

    const std::vector<std::int64_t> packed = compactBitStorage::pack(values, 5);
    CHECK(packed.size() == compactBitStorage::longsFor(values.size(), 5));
    CHECK(packed.size() == 4);
    CHECK(compactBitStorage::unpack(packed, 5, values.size()) == values);
}

TEST_CASE("a chain of tables renumbers once, not once per link") {
    // Two links: the first slides 100..109 down by ten, the second slides the result down by five.
    VersionTables first;
    first.older = 775;
    first.blockStates = IdMap{"100:10:-10|"};
    first.newerPlayerEntityType = 156;
    first.newEntries = NameSet{"minecraft:waypoint_style/*"};

    VersionTables second;
    second.older = 774;
    second.blockStates = IdMap{"90:10:-5|"};
    second.playerEntityType = 128;
    second.blockStateDirectBits = 14;
    second.attributeNames = NameIdMap{"generic.max_health:0"};

    const std::vector<std::reference_wrapper<const VersionTables>> chain = {first, second};
    const ComposedTables tables{chain};

    CHECK(tables.blockStates.map(100) == 85);
    CHECK(tables.blockStates.unmap(85) == 100);
    // A step with nothing to show stops the walk rather than passing a wrong number down.
    CHECK(tables.blockStates.map(5) == IdLookup::kNone);

    CHECK(tables.blockStateDirectBits() == 14);
    CHECK(tables.attributeNames().id("generic.max_health") == 0);
    CHECK(tables.isPlayer(128));
    CHECK_FALSE(tables.isPlayer(156));
    CHECK(tables.nativePlayerEntityType() == 156);
    CHECK_FALSE(tables.fluidCountInSections());

    // A whole registry the far end lacks is named with a star, and a command tree naming it would cost the connection.
    CHECK(tables.missingRegistries().contains("minecraft:waypoint_style"));
}

TEST_CASE("packet id tables translate both ways and drop what has no name") {
    const PacketIdMap map{"5,-1,3"};
    CHECK(map.map(0) == 5);
    CHECK(map.map(1) == PacketIdMap::kDropped);
    CHECK(map.map(2) == 3);
    CHECK(map.map(99) == PacketIdMap::kDropped);
    CHECK(map.unmap(5) == 0);
    CHECK(map.unmap(3) == 2);
    CHECK(map.unmap(4) == PacketIdMap::kDropped);

    const PacketIdTables tables;
    CHECK(tables.toOlder(basalt::wire::protocol::ProtocolState::Play, 7) == 7);
    CHECK_FALSE(tables.hasClientbound(basalt::wire::protocol::ProtocolState::Play));
}

TEST_CASE("a table read upward undoes what it did downward") {
    const IdMap down{"0:10:0;12:5:-2|10>3;11>3"};
    const IdMap up = down.inverted();
    for (std::int32_t id : {0, 4, 9, 12, 16}) {
        CHECK(up.map(down.map(id)) == id);
    }
    CHECK(up.map(99) == IdMap::kNone);

    const PacketIdMap packets{"5,-1,3"};
    CHECK(packets.inverted().map(5) == 0);
    CHECK(packets.inverted().unmap(2) == 3);
}

TEST_CASE("a move packet goes down to 26.2 and comes back up to 26.3 as it was") {
    namespace java = basalt::wire::multiversion::java;
    using basalt::wire::protocol::ProtocolState;
    basalt::wire::io::ByteBuffer native;
    basalt::wire::protocol::varInt::write(native, 7);
    basalt::wire::protocol::varInt::write(native, 1 | (1 << 1));
    basalt::wire::protocol::varInt::write(native, 3);
    native.writeShort(100);
    native.writeShort(-50);
    native.writeShort(25);
    const std::vector<std::uint8_t> original = native.toVector();

    const auto down = java::javaTranslators::chainFor(776);
    const auto up = java::javaTranslators::upChainFor(776);
    REQUIRE(down);
    REQUIRE(up);
    std::int32_t olderId = -1;
    std::vector<std::uint8_t> older;
    down->chain().clientbound(ProtocolState::Play, 54, original,
                              [&](std::int32_t id, std::span<const std::uint8_t> body) {
                                  olderId = id;
                                  older.assign(body.begin(), body.end());
                              });
    REQUIRE(olderId >= 0);
    CHECK(older.size() == 1 + 6 + 1);

    std::int32_t backId = -1;
    std::vector<std::uint8_t> back;
    up->chain().clientbound(ProtocolState::Play, olderId, older,
                            [&](std::int32_t id, std::span<const std::uint8_t> body) {
                                backId = id;
                                back.assign(body.begin(), body.end());
                            });
    CHECK(backId == 54);
    CHECK(back == original);
}

TEST_CASE("the chunk rewriter renumbers block states and leaves biomes alone") {
    using namespace basalt::wire;
    namespace paletted = protocol::play::palettedContainer;

    std::vector<std::int32_t> blocks(4096, 0);
    for (std::size_t i = 0; i < 64; ++i) {
        blocks[i] = 100;
    }
    const std::vector<std::int32_t> biomes(64, 3);

    io::ByteBuffer section;
    section.writeShort(64);
    section.writeShort(0);
    paletted::write(section, blocks, paletted::kBlockMinIndirectBits, paletted::kBlockMaxIndirectBits, 15);
    paletted::write(section, biomes, paletted::kBiomeMinIndirectBits, paletted::kBiomeMaxIndirectBits, 6);

    const IdMap states{"100:1:-10|"};
    ChunkRewriter::Options options;
    options.olderDirectBits = 14;
    options.fallbackState = 0;
    ChunkRewriter rewriter{states, options};

    const std::vector<std::uint8_t> rewritten = rewriter.rewrite(section.toVector());
    REQUIRE_FALSE(rewritten.empty());

    io::ByteBuffer read = io::ByteBuffer::wrap(rewritten);
    CHECK(read.readShort() == 64);
    CHECK(read.readShort() == 0);
    const std::vector<std::int32_t> readBlocks = paletted::read(read, 4096, paletted::kBlockMaxIndirectBits);
    REQUIRE(readBlocks.size() == 4096);
    CHECK(readBlocks[0] == 90);
    // A state the far end has no word for becomes its air rather than a wrong block.
    CHECK(readBlocks[100] == 0);

    // The biomes come back byte for byte: both sides already agree on their numbering.
    const std::vector<std::int32_t> readBiomes = paletted::read(read, 64, paletted::kBiomeMaxIndirectBits);
    CHECK(readBiomes == biomes);
    CHECK(read.readableBytes() == 0);
}

namespace {

class TableOnlyLink final : public ProtocolLink {
public:
    TableOnlyLink(std::int32_t older, PacketIdTables tables) : older_(older), tables_(std::move(tables)) {}

    [[nodiscard]] std::int32_t older() const override { return older_; }

    [[nodiscard]] const PacketIdTables& packets() const override { return tables_; }

    [[nodiscard]] bool clientbound(basalt::wire::protocol::ProtocolState, MutablePacket&) override { return true; }

    [[nodiscard]] bool serverbound(basalt::wire::protocol::ProtocolState, MutablePacket&) override { return true; }

private:
    std::int32_t older_;
    PacketIdTables tables_;
};

} // namespace

TEST_CASE("a chain renumbers a packet through every link and drops what has no name") {
    using basalt::wire::protocol::ProtocolState;

    // Two links: the first calls native 0 "5", the second calls that "2".
    std::map<ProtocolState, PacketIdMap> firstClient;
    firstClient.emplace(ProtocolState::Play, PacketIdMap{"5,-1"});
    std::map<ProtocolState, PacketIdMap> firstServer;
    firstServer.emplace(ProtocolState::Play, PacketIdMap{"5,-1"});
    TableOnlyLink first{775, PacketIdTables{firstClient, firstServer}};

    std::map<ProtocolState, PacketIdMap> secondClient;
    secondClient.emplace(ProtocolState::Play, PacketIdMap{"0,1,2,3,4,2"});
    std::map<ProtocolState, PacketIdMap> secondServer;
    secondServer.emplace(ProtocolState::Play, PacketIdMap{"0,1,2,3,4,2"});
    TableOnlyLink second{774, PacketIdTables{secondClient, secondServer}};

    std::vector<std::reference_wrapper<ProtocolLink>> links = {first, second};
    const ProtocolChain chain{links};

    std::vector<std::pair<std::int32_t, std::vector<std::uint8_t>>> sent;
    const ProtocolChain::Emit collect = [&sent](std::int32_t id, std::span<const std::uint8_t> body) {
        sent.emplace_back(id, std::vector<std::uint8_t>{body.begin(), body.end()});
    };

    chain.clientbound(ProtocolState::Play, 0, std::vector<std::uint8_t>{1, 2, 3}, collect);
    REQUIRE(sent.size() == 1);
    CHECK(sent[0].first == 2);
    CHECK(sent[0].second == std::vector<std::uint8_t>{1, 2, 3});

    sent.clear();
    chain.clientbound(ProtocolState::Play, 1, std::vector<std::uint8_t>{}, collect);
    CHECK(sent.empty());

    sent.clear();
    chain.serverbound(ProtocolState::Play, 2, std::vector<std::uint8_t>{9}, collect);
    REQUIRE(sent.size() == 1);
    CHECK(sent[0].first == 0);
}

TEST_CASE("1.8 writes the kind and the index in one byte and leaves out what it has no kind for") {
    using basalt::wire::protocol::play::SetEntityDataPacket;

    const java::EntityDataRewriter rewriter{
        protocolVersions::kV1_8, {protocolVersions::kV1_8}, [](const auto& stack) { return stack; }};
    SetEntityDataPacket data;
    data.entityId = 1;
    data.entries.push_back({0, SetEntityDataPacket::ByteValue{0x21}});
    data.entries.push_back({1, SetEntityDataPacket::IntValue{300}});

    const auto body = rewriter.rewrite(data, 5);
    REQUIRE(body.has_value());
    CHECK(*body == std::vector<std::uint8_t>{0x01, 0x00, 0x21, 0x7F});
}

namespace {

std::vector<std::uint8_t> clientbound(const ProtocolChain& chain, std::int32_t id,
                                      const std::vector<std::uint8_t>& body) {
    std::vector<std::uint8_t> out;
    chain.clientbound(
        basalt::wire::protocol::ProtocolState::Play, id, body,
        [&out](std::int32_t, std::span<const std::uint8_t> bytes) { out.assign(bytes.begin(), bytes.end()); });
    return out;
}

} // namespace

TEST_CASE("a chain for 26.2 gives back the shapes 26.3 changed") {
    namespace play = basalt::wire::protocol::play;

    const std::unique_ptr<java::JavaChain> built = java::javaTranslators::chainFor(protocolVersions::kV26_2);
    REQUIRE(built != nullptr);
    const ProtocolChain& chain = built->chain();

    SUBCASE("a swing goes back into the animate packet") {
        basalt::wire::io::ByteBuffer body;
        play::SwingAnimationPacket{7, 1, {}}.write(body);
        std::int32_t sentId = -1;
        std::vector<std::uint8_t> sent;
        chain.clientbound(basalt::wire::protocol::ProtocolState::Play, play::SwingAnimationPacket::kId, body.toVector(),
                          [&](std::int32_t id, std::span<const std::uint8_t> bytes) {
                              sentId = id;
                              sent.assign(bytes.begin(), bytes.end());
                          });
        CHECK(sentId == play::AnimatePacket::kId);
        CHECK(sent == std::vector<std::uint8_t>{7, 3});
    }

    SUBCASE("a respawn writes its game modes as the two bytes 26.2 reads") {
        play::SpawnInfo spawn;
        spawn.dimensionName = "minecraft:overworld";
        spawn.gamemode = play::Gamemode::Creative;
        basalt::wire::io::ByteBuffer body;
        play::RespawnPacket{spawn, 0}.write(body);
        basalt::wire::io::ByteBuffer out =
            basalt::wire::io::ByteBuffer::wrap(clientbound(chain, play::RespawnPacket::kId, body.toVector()));
        const java::LoginFields::SpawnFields read = java::LoginFields::SpawnFields::read(out, true);
        CHECK(read.gamemode == 1);
        CHECK(read.previousGamemode == 0xFF);
        CHECK(out.readByte() == 0);
        CHECK_FALSE(out.failed());
        CHECK(out.readableBytes() == 0);
    }

    SUBCASE("the light masks go back to longs") {
        play::UpdateLightPacket light;
        light.chunkX = 1;
        light.chunkZ = 2;
        light.light.skyLightMask = {0x102};
        basalt::wire::io::ByteBuffer body;
        light.write(body);
        basalt::wire::io::ByteBuffer out =
            basalt::wire::io::ByteBuffer::wrap(clientbound(chain, play::UpdateLightPacket::kId, body.toVector()));
        CHECK(basalt::wire::protocol::varInt::read(out) == 1);
        CHECK(basalt::wire::protocol::varInt::read(out) == 2);
        const auto read = java::readLongMaskLight(out);
        REQUIRE(read.has_value());
        CHECK(read->skyLightMask == std::vector<std::int64_t>{0x102});
    }
}

TEST_CASE("an advancement reaches an older client in the layout that client reads") {
    namespace play = basalt::wire::protocol::play;
    namespace buffer = basalt::wire::protocol;
    using basalt::wire::io::ByteBuffer;

    play::UpdateAdvancementsPacket update;
    update.reset = true;
    play::AdvancementEntry entry;
    entry.id = "minecraft:story/root";
    entry.display = play::AdvancementDisplayData{.title = basalt::wire::text::Component::text("Root"),
                                                 .icon = play::WireItemStack{.itemId = 1, .count = 1},
                                                 .background = "minecraft:gui/advancements/backgrounds/stone"};
    entry.requirements = {{"a", "b"}};
    entry.sendsTelemetryEvent = true;
    entry.x = 1.5F;
    entry.y = 2.5F;
    update.added.push_back(entry);
    ByteBuffer body;
    update.write(body);

    const auto through = [&body](std::int32_t version, bool& emitted) {
        const std::unique_ptr<java::JavaChain> built = java::javaTranslators::chainFor(version);
        REQUIRE(built != nullptr);
        std::vector<std::uint8_t> out;
        emitted = false;
        built->chain().clientbound(basalt::wire::protocol::ProtocolState::Play, play::UpdateAdvancementsPacket::kId,
                                   body.toVector(), [&](std::int32_t, std::span<const std::uint8_t> bytes) {
                                       emitted = true;
                                       out.assign(bytes.begin(), bytes.end());
                                   });
        return out;
    };

    SUBCASE("26.2 keeps the position inside the display") {
        bool emitted = false;
        ByteBuffer in = ByteBuffer::wrap(through(protocolVersions::kV26_2, emitted));
        REQUIRE(emitted);
        CHECK(buffer::readBoolean(in));
        CHECK(buffer::varInt::read(in) == 1);
        CHECK(buffer::readString(in) == "minecraft:story/root");
        CHECK_FALSE(buffer::readBoolean(in));
        REQUIRE(buffer::readBoolean(in));
        static_cast<void>(buffer::readAnonymousNbt(in));
        static_cast<void>(buffer::readAnonymousNbt(in));
        CHECK(buffer::varInt::read(in) == 1);
        CHECK(buffer::varInt::read(in) == 1);
        CHECK(buffer::varInt::read(in) == 0);
        CHECK(buffer::varInt::read(in) == 0);
        CHECK(buffer::varInt::read(in) == 0);
        CHECK(in.readInt() == 3);
        CHECK(buffer::readString(in) == "minecraft:gui/advancements/backgrounds/stone");
        CHECK(in.readFloat() == 1.5F);
        CHECK(in.readFloat() == 2.5F);
    }

    SUBCASE("1.12.2 gets json text, a damaged stack, the texture path and the criteria by name") {
        bool emitted = false;
        ByteBuffer in = ByteBuffer::wrap(through(protocolVersions::kV1_12_2, emitted));
        REQUIRE(emitted);
        CHECK(buffer::readBoolean(in));
        CHECK(buffer::varInt::read(in) == 1);
        CHECK(buffer::readString(in) == "minecraft:story/root");
        CHECK_FALSE(buffer::readBoolean(in));
        REQUIRE(buffer::readBoolean(in));
        CHECK(buffer::readString(in).find("Root") != std::string::npos);
        static_cast<void>(buffer::readString(in));
        CHECK(in.readShort() == 1);
        CHECK(in.readByte() == 1);
        CHECK(in.readShort() == 0);
        CHECK(in.readByte() == 0);
        CHECK(buffer::varInt::read(in) == 0);
        CHECK(in.readInt() == 3);
        CHECK(buffer::readString(in) == "minecraft:textures/gui/advancements/backgrounds/stone.png");
        CHECK(in.readFloat() == 1.5F);
        CHECK(in.readFloat() == 2.5F);
        CHECK(buffer::varInt::read(in) == 2);
        CHECK(buffer::readString(in) == "a");
        CHECK(buffer::readString(in) == "b");
        CHECK(buffer::varInt::read(in) == 1);
        CHECK(buffer::varInt::read(in) == 2);
        static_cast<void>(buffer::readString(in));
        static_cast<void>(buffer::readString(in));
        CHECK(buffer::varInt::read(in) == 0);
        CHECK(buffer::varInt::read(in) == 0);
        CHECK_FALSE(in.failed());
        CHECK(in.readableBytes() == 0);
    }

    SUBCASE("1.11 has no advancements, so it gets nothing") {
        bool emitted = true;
        static_cast<void>(through(protocolVersions::kV1_11_1, emitted));
        CHECK_FALSE(emitted);
    }
}

TEST_CASE("a map reaches an older client in the layout and palette that client has") {
    namespace play = basalt::wire::protocol::play;
    namespace buffer = basalt::wire::protocol;
    using basalt::wire::io::ByteBuffer;

    play::MapItemDataPacket map;
    map.mapId = 3;
    map.scale = 1;
    map.locked = true;
    map.decorations = std::vector<play::MapDecorationEntry>{
        {.type = 0, .x = 4, .y = 5, .rotation = 7}, {.type = 10, .x = 1, .y = 2}, {.type = 36, .x = 3, .y = 3}};
    // Deepslate's normal shade, which only 1.17 and up can draw.
    map.patch = play::MapColorPatch{.width = 1, .height = 1, .colors = {59 * 4 + 1}};
    ByteBuffer body;
    map.write(body);

    const auto through = [&body](std::int32_t version) {
        const std::unique_ptr<java::JavaChain> built = java::javaTranslators::chainFor(version);
        REQUIRE(built != nullptr);
        std::vector<std::uint8_t> out;
        built->chain().clientbound(
            basalt::wire::protocol::ProtocolState::Play, play::MapItemDataPacket::kId, body.toVector(),
            [&](std::int32_t, std::span<const std::uint8_t> bytes) { out.assign(bytes.begin(), bytes.end()); });
        return out;
    };

    SUBCASE("26.2 draws the new icon as the red cross") {
        ByteBuffer in = ByteBuffer::wrap(through(protocolVersions::kV26_2));
        const auto read = play::MapItemDataPacket::read(in);
        REQUIRE(read.has_value());
        REQUIRE(read->decorations.has_value());
        CHECK(read->decorations->at(2).type == 26);
        CHECK(read->patch->colors == std::vector<std::uint8_t>{59 * 4 + 1});
    }

    SUBCASE("1.16.4 has the tracking flag, a bare count and no deepslate") {
        ByteBuffer in = ByteBuffer::wrap(through(protocolVersions::kV1_16_4));
        CHECK(buffer::varInt::read(in) == 3);
        CHECK(in.readByte() == 1);
        CHECK(buffer::readBoolean(in));
        CHECK(buffer::readBoolean(in));
        CHECK(buffer::varInt::read(in) == 3);
        for (int i = 0; i < 3; ++i) {
            static_cast<void>(buffer::varInt::read(in));
            static_cast<void>(in.readByte());
            static_cast<void>(in.readByte());
            static_cast<void>(in.readByte());
            CHECK_FALSE(buffer::readBoolean(in));
        }
        CHECK(in.readByte() == 1);
        static_cast<void>(in.readByte());
        static_cast<void>(in.readByte());
        static_cast<void>(in.readByte());
        const std::vector<std::uint8_t> colors = buffer::readByteArray(in);
        REQUIRE(colors.size() == 1);
        CHECK(colors[0] < 59 * 4);
        CHECK(colors[0] >= 4);
        CHECK_FALSE(in.failed());
        CHECK(in.readableBytes() == 0);
    }

    SUBCASE("1.12.2 packs type and rotation in a byte and drops what it has no icon for") {
        ByteBuffer in = ByteBuffer::wrap(through(protocolVersions::kV1_12_2));
        CHECK(buffer::varInt::read(in) == 3);
        CHECK(in.readByte() == 1);
        CHECK(buffer::readBoolean(in));
        CHECK(buffer::varInt::read(in) == 1);
        CHECK(in.readByte() == 7);
        CHECK(in.readByte() == 4);
        CHECK(in.readByte() == 5);
        CHECK(in.readByte() == 1);
    }
}

TEST_CASE("every version the gate admits has an unbroken road of links") {
    for (const std::int32_t version : java::javaTranslators::supported()) {
        if (version == protocolVersions::kNative) {
            continue;
        }
        CAPTURE(version);
        CHECK(java::javaTranslators::chainFor(version) != nullptr);
    }
    CHECK(java::javaTranslators::supported().contains(protocolVersions::kV1_8));
}
