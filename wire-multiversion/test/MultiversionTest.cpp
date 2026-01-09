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
#include <basalt/wire/multiversion/java/Tables775.hpp>
#include <basalt/wire/protocol/ProtocolBuffer.hpp>
#include <basalt/wire/protocol/play/EntityPackets.hpp>
#include <basalt/wire/protocol/play/InventoryPackets.hpp>
#include <basalt/wire/protocol/play/PalettedContainer.hpp>
#include <basalt/wire/protocol/play/WorldPackets.hpp>

#include <doctest.h>

#include <cstdint>
#include <map>
#include <vector>

using namespace basalt::wire::multiversion;

TEST_CASE("an id map reads runs of constant difference") {
    // Two runs and one hole: ids 0 to 9 stay put, 20 to 29 slide down by two, and 100 is shown as 7.
    // The runs are chosen so their destinations do not overlap, which is what a generated map looks
    // like: reading one backwards is only unambiguous when they do not.
    const IdMap map{"0:10:0;20:10:-2|100>7"};

    CHECK(map.map(0) == 0);
    CHECK(map.map(9) == 9);
    CHECK(map.map(20) == 18);
    CHECK(map.map(29) == 27);
    CHECK(map.map(100) == 7);
    CHECK(map.substitutes(100));
    CHECK_FALSE(map.substitutes(20));
    // Nothing on the other side and no substitute either.
    CHECK(map.map(50) == IdLookup::kNone);

    CHECK(map.unmap(18) == 20);
    CHECK(map.unmap(0) == 0);
    CHECK(map.unmap(1000) == IdLookup::kNone);

    // A registry a release left alone changes nothing at all.
    CHECK(IdMap::identity().map(12345) == 12345);
    CHECK(IdMap::identity().unmap(12345) == 12345);
}

TEST_CASE("a name map answers both ways, and a name may carry colons") {
    const NameIdMap map{"minecraft:desert:2;minecraft:plains:1;minecraft:the_void:0"};

    CHECK(map.id("minecraft:plains") == 1);
    CHECK(map.id("minecraft:desert") == 2);
    CHECK(map.id("minecraft:nether_wastes") == NameIdMap::kNone);
    CHECK(map.name(0) == "minecraft:the_void");
    CHECK(map.name(99).empty());
    CHECK_FALSE(map.empty());
    CHECK(NameIdMap::empty_().empty());
}

TEST_CASE("a name set is a membership question, sorted on first use") {
    const NameSet set{"minecraft:zombie;minecraft:allay;minecraft:camel"};

    CHECK(set.contains("minecraft:allay"));
    CHECK(set.contains("minecraft:zombie"));
    CHECK_FALSE(set.contains("minecraft:creeper"));
    CHECK(set.names().size() == 3);
    // Sorted, because that is what makes the membership check a binary search.
    CHECK(set.names().front() == "minecraft:allay");
    CHECK(NameSet{""}.empty());
}

TEST_CASE("the pre-1.16 packing spills entries across word boundaries") {
    // Five bits into 64 leaves four bits over, so the thirteenth entry straddles two words. That
    // straddle is exactly what 1.16 stopped doing and what every link below it has to write back.
    std::vector<std::int32_t> values(40);
    for (std::size_t i = 0; i < values.size(); ++i) {
        values[i] = static_cast<std::int32_t>(i % 32);
    }

    const std::vector<std::int64_t> packed = compactBitStorage::pack(values, 5);
    CHECK(packed.size() == compactBitStorage::longsFor(values.size(), 5));
    // Tight packing: 40 entries of 5 bits is 200 bits, which is four words, not the five the
    // post-1.16 scheme would need.
    CHECK(packed.size() == 4);
    CHECK(compactBitStorage::unpack(packed, 5, values.size()) == values);
}

TEST_CASE("the protocol table knows its versions and says so about the ones it does not") {
    CHECK(protocolVersions::oldest() == protocolVersions::kV1_8);
    CHECK(protocolVersions::of(protocolVersions::kV1_16_4)->label == "1.16.4-1.16.5");
    CHECK_FALSE(protocolVersions::of(999).has_value());

    CHECK(protocolVersions::describe(47) == "1.8-1.8.9 (47)");
    CHECK(protocolVersions::describe(999) == "unknown (999)");
    CHECK(protocolVersions::describe(protocolVersions::kSnapshotBit | 300) == "snapshot 300 (1073742124)");

    CHECK(protocolVersions::isSnapshot(protocolVersions::kSnapshotBit | 1));
    CHECK(protocolVersions::isFuture(protocolVersions::kNative + 1));
    CHECK_FALSE(protocolVersions::isFuture(protocolVersions::kSnapshotBit | 1));
    CHECK(protocolVersions::isBelowFloor(5));
}

TEST_CASE("the gate answers before a player exists") {
    const ProtocolGate gate{{protocolVersions::kV1_8, protocolVersions::kV1_20},
                            UnknownProtocolPolicy::Warn,
                            UnknownProtocolPolicy::Refuse};

    CHECK(gate.verdictFor(protocolVersions::kNative) == ProtocolVerdict{ProtocolVerdict::Native{}});
    CHECK(std::holds_alternative<ProtocolVerdict::Translated>(gate.verdictFor(protocolVersions::kV1_8).value));

    // A version the table knows but no link handles is refused for that reason, not as too old.
    const auto untranslated = gate.verdictFor(protocolVersions::kV1_12_2);
    REQUIRE(std::holds_alternative<ProtocolVerdict::Refused>(untranslated.value));
    CHECK(std::get<ProtocolVerdict::Refused>(untranslated.value).reason ==
          ProtocolVerdict::Refused::Reason::NotTranslated);

    // A future release is a bet the operator chose to take, and the log gets told it is one.
    const auto future = gate.verdictFor(protocolVersions::kNative + 1);
    REQUIRE(std::holds_alternative<ProtocolVerdict::Unknown>(future.value));
    CHECK(std::get<ProtocolVerdict::Unknown>(future.value).warn);

    // A snapshot is refused under this policy, and as a snapshot rather than as too new.
    const auto snapshot = gate.verdictFor(protocolVersions::kSnapshotBit | 400);
    REQUIRE(std::holds_alternative<ProtocolVerdict::Refused>(snapshot.value));
    CHECK(std::get<ProtocolVerdict::Refused>(snapshot.value).reason == ProtocolVerdict::Refused::Reason::Snapshot);

    // Below the floor the numbers stop being comparable at all: 1.4.2 is also 47.
    const auto ancient = gate.verdictFor(5);
    REQUIRE(std::holds_alternative<ProtocolVerdict::Refused>(ancient.value));
    CHECK(std::get<ProtocolVerdict::Refused>(ancient.value).reason == ProtocolVerdict::Refused::Reason::TooOld);

    CHECK(parseUnknownProtocolPolicy(" REFUSE ") == UnknownProtocolPolicy::Refuse);
    CHECK_FALSE(parseUnknownProtocolPolicy("maybe").has_value());
}

TEST_CASE("a chain of tables renumbers once, not once per link") {
    // Two links: the first slides 100..109 down by ten, the second slides the result down by five.
    // Composed, an id walks both in one call, which is the whole point: a link halfway down must
    // never see a packet an earlier link already rewrote.
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

    // The far end's numbering and names are what a link ends up writing against.
    CHECK(tables.blockStateDirectBits() == 14);
    CHECK(tables.attributeNames().id("generic.max_health") == 0);
    CHECK(tables.isPlayer(128));
    CHECK_FALSE(tables.isPlayer(156));
    CHECK(tables.nativePlayerEntityType() == 156);
    // 26.1 added the fluid count, so a chain ending below it does not read one.
    CHECK_FALSE(tables.fluidCountInSections());

    // A whole registry the far end lacks is named with a star, and a command tree naming it would
    // cost the connection.
    CHECK(tables.missingRegistries().contains("minecraft:waypoint_style"));
}

TEST_CASE("packet id tables translate both ways and drop what has no name") {
    // Native ids 0, 1 and 2 become 5, -1 and 3: the middle one has no word in the older version.
    const PacketIdMap map{"5,-1,3"};
    CHECK(map.map(0) == 5);
    CHECK(map.map(1) == PacketIdMap::kDropped);
    CHECK(map.map(2) == 3);
    CHECK(map.map(99) == PacketIdMap::kDropped);
    CHECK(map.unmap(5) == 0);
    CHECK(map.unmap(3) == 2);
    CHECK(map.unmap(4) == PacketIdMap::kDropped);

    // A phase with no table left everything where it was.
    const PacketIdTables tables;
    CHECK(tables.toOlder(basalt::wire::protocol::ProtocolState::Play, 7) == 7);
    CHECK_FALSE(tables.hasClientbound(basalt::wire::protocol::ProtocolState::Play));
}

TEST_CASE("the chunk rewriter renumbers block states and leaves biomes alone") {
    using namespace basalt::wire;
    namespace paletted = protocol::play::palettedContainer;

    // One section: a block count, a fluid count, a block container and a biome container.
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

    // The older side numbers state 100 as 90 and has nothing at all for anything else.
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

TEST_CASE("a length-prefixed palette counts its longs out loud") {
    using namespace basalt::wire;
    namespace paletted = protocol::play::palettedContainer;

    // Up to 1.21.4 the data array is preceded by its length; a client that expects that number and
    // does not get it reads the next field as a length.
    const std::vector<std::int32_t> biomes(64, 3);
    io::ByteBuffer section;
    paletted::write(section, biomes, paletted::kBiomeMinIndirectBits, paletted::kBiomeMaxIndirectBits, 6);

    ChunkRewriter::Options options;
    options.lengthPrefixedData = true;
    ChunkRewriter rewriter{IdMap::identity(), options};

    const std::vector<std::uint8_t> rewritten = rewriter.rewriteBiomes(section.toVector());
    io::ByteBuffer read = io::ByteBuffer::wrap(rewritten);
    // A single-valued container: zero bits, the value, and then a zero count that the modern form
    // does not write at all.
    CHECK(read.readByte() == 0);
    CHECK(protocol::varInt::read(read) == 3);
    CHECK(protocol::varInt::read(read) == 0);
    CHECK(read.readableBytes() == 0);
}

namespace {

/// A link that only renumbers, which is what most modern links are.
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

/// A link that turns one packet into two, which is what the awkward ones do.
class SplittingLink final : public ProtocolLink {
public:
    [[nodiscard]] std::int32_t older() const override { return 1; }

    [[nodiscard]] const PacketIdTables& packets() const override { return tables_; }

    [[nodiscard]] bool clientbound(basalt::wire::protocol::ProtocolState, MutablePacket& packet) override {
        packet.extras.emplace_back(99, std::vector<std::uint8_t>{0xAA});
        return true;
    }

    [[nodiscard]] bool serverbound(basalt::wire::protocol::ProtocolState, MutablePacket&) override { return true; }

private:
    PacketIdTables tables_;
};

} // namespace

TEST_CASE("a chain renumbers a packet through every link and drops what has no name") {
    using basalt::wire::protocol::ProtocolState;

    // Two links: the first calls native 0 "5", the second calls that "2". Native 1 has no word in
    // the first link at all.
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

    // A packet the far end has no word for is thrown away rather than sent as something else.
    sent.clear();
    chain.clientbound(ProtocolState::Play, 1, std::vector<std::uint8_t>{}, collect);
    CHECK(sent.empty());

    // Serverbound walks the same links the other way.
    sent.clear();
    chain.serverbound(ProtocolState::Play, 2, std::vector<std::uint8_t>{9}, collect);
    REQUIRE(sent.size() == 1);
    CHECK(sent[0].first == 0);
}

TEST_CASE("a link that makes a second packet has it renumbered like any other") {
    using basalt::wire::protocol::ProtocolState;

    SplittingLink splitting;
    std::map<ProtocolState, PacketIdMap> table;
    // The link below renames 99 to 7 and leaves 0 where it is.
    table.emplace(ProtocolState::Play, PacketIdMap{"0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23,"
                                                   "24,25,26,27,28,29,30,31,32,33,34,35,36,37,38,39,40,41,42,43,44,"
                                                   "45,46,47,48,49,50,51,52,53,54,55,56,57,58,59,60,61,62,63,64,65,"
                                                   "66,67,68,69,70,71,72,73,74,75,76,77,78,79,80,81,82,83,84,85,86,"
                                                   "87,88,89,90,91,92,93,94,95,96,97,98,7"});
    TableOnlyLink below{773, PacketIdTables{table, table}};

    std::vector<std::reference_wrapper<ProtocolLink>> links = {splitting, below};
    const ProtocolChain chain{links};

    std::vector<std::pair<std::int32_t, std::vector<std::uint8_t>>> sent;
    chain.clientbound(ProtocolState::Play, 0, std::vector<std::uint8_t>{1},
                      [&sent](std::int32_t id, std::span<const std::uint8_t> body) {
                          sent.emplace_back(id, std::vector<std::uint8_t>{body.begin(), body.end()});
                      });

    REQUIRE(sent.size() == 2);
    CHECK(sent[0].first == 0);
    // The extra was made at the first link and the second one renumbered it, exactly as it would
    // have renumbered the packet it came from.
    CHECK(sent[1].first == 7);
    CHECK(sent[1].second == std::vector<std::uint8_t>{0xAA});
}

TEST_CASE("1.8 writes the kind and the index in one byte and leaves out what it has no kind for") {
    using basalt::wire::protocol::play::SetEntityDataPacket;

    const java::EntityDataRewriter rewriter{
        protocolVersions::kV1_8, {protocolVersions::kV1_8}, [](const auto& stack) { return stack; }};
    SetEntityDataPacket data;
    data.entityId = 1;
    data.entries.push_back({0, SetEntityDataPacket::ByteValue{0x21}});
    // A varint is a kind 1.8 never had, and this one has no other shape to be written as.
    data.entries.push_back({1, SetEntityDataPacket::IntValue{300}});

    const auto body = rewriter.rewrite(data, 5);
    REQUIRE(body.has_value());
    // The entity, then the packed pair 0x00 with its byte, then 1.8's own end of list.
    CHECK(*body == std::vector<std::uint8_t>{0x01, 0x00, 0x21, 0x7F});
}

TEST_CASE("a boolean is written as the byte it always was where there is no kind for one") {
    using basalt::wire::protocol::play::SetEntityDataPacket;

    const java::EntityDataRewriter old{
        protocolVersions::kV1_8, {protocolVersions::kV1_8}, [](const auto& stack) { return stack; }};
    SetEntityDataPacket data;
    data.entityId = 1;
    data.entries.push_back({1, SetEntityDataPacket::BoolValue{true}});

    const auto packed = old.rewrite(data, 5);
    REQUIRE(packed.has_value());
    CHECK(*packed == std::vector<std::uint8_t>{0x01, 0x01, 0x01, 0x7F});

    // A version that has the kind says so, with the index and the kind written apart.
    const java::EntityDataRewriter modern{
        protocolVersions::kV1_21_6, {protocolVersions::kV1_21_6}, [](const auto& stack) { return stack; }};
    const auto unpacked = modern.rewrite(data, 5);
    REQUIRE(unpacked.has_value());
    CHECK(*unpacked == std::vector<std::uint8_t>{0x01, 0x01, 0x08, 0x01, 0xFF});
}

namespace {

/// The numbers are read out of the two versions' own reports (26.1.2 and 26.2), so a failure here
/// means the table is wrong rather than the expectation being stale.
constexpr std::int32_t kStone776 = 1;
constexpr std::int32_t kStone775 = 1;
constexpr std::int32_t kOakPlanks776 = 63;
constexpr std::int32_t kOakPlanks775 = 36;
constexpr std::int32_t kMaxHealth776 = 23;
constexpr std::int32_t kMaxHealth775 = 19;
constexpr std::int32_t kBounciness776 = 9;

/// Runs one packet down a chain and hands back what came out the far end.
std::vector<std::uint8_t> clientbound(const ProtocolChain& chain, std::int32_t id,
                                      const std::vector<std::uint8_t>& body) {
    std::vector<std::uint8_t> out;
    chain.clientbound(
        basalt::wire::protocol::ProtocolState::Play, id, body,
        [&out](std::int32_t, std::span<const std::uint8_t> bytes) { out.assign(bytes.begin(), bytes.end()); });
    return out;
}

} // namespace

TEST_CASE("a chain for 26.1 renumbers what a packet carries") {
    namespace play = basalt::wire::protocol::play;

    const std::unique_ptr<java::JavaChain> built = java::javaTranslators::chainFor(protocolVersions::kV26_1);
    REQUIRE(built != nullptr);
    const ProtocolChain& chain = built->chain();

    SUBCASE("a block state keeps its number where nothing moved") {
        play::ClientboundBlockUpdatePacket update;
        update.pos = {8, 64, 8};
        update.blockStateId = kStone776;
        basalt::wire::io::ByteBuffer body;
        update.write(body);

        basalt::wire::io::ByteBuffer out = basalt::wire::io::ByteBuffer::wrap(
            clientbound(chain, play::ClientboundBlockUpdatePacket::kId, body.toVector()));
        const auto sent = play::ClientboundBlockUpdatePacket::read(out);
        REQUIRE(sent.has_value());
        CHECK(sent->blockStateId == kStone775);
    }

    SUBCASE("an item moves to where 26.1 keeps it") {
        play::ContainerSetSlotPacket slot;
        slot.stateId = 1;
        slot.slot = 5;
        slot.item = play::WireItemStack{.itemId = kOakPlanks776, .count = 3};
        basalt::wire::io::ByteBuffer body;
        slot.write(body);

        basalt::wire::io::ByteBuffer out =
            basalt::wire::io::ByteBuffer::wrap(clientbound(chain, play::ContainerSetSlotPacket::kId, body.toVector()));
        const auto sent = play::ContainerSetSlotPacket::read(out);
        REQUIRE(sent.has_value());
        REQUIRE(sent->item.has_value());
        CHECK(sent->item->itemId == kOakPlanks775);
        CHECK(sent->item->count == 3);
    }

    SUBCASE("an attribute 26.1 does not have is left out instead of sent wrong") {
        play::UpdateAttributesPacket attributes;
        attributes.entityId = 7;
        attributes.attributes.push_back({.attribute = kMaxHealth776, .base = 20.0});
        attributes.attributes.push_back({.attribute = kBounciness776, .base = 1.0});
        basalt::wire::io::ByteBuffer body;
        attributes.write(body);

        basalt::wire::io::ByteBuffer out =
            basalt::wire::io::ByteBuffer::wrap(clientbound(chain, play::UpdateAttributesPacket::kId, body.toVector()));
        const auto sent = play::UpdateAttributesPacket::read(out);
        REQUIRE(sent.has_value());
        REQUIRE(sent->attributes.size() == 1);
        CHECK(sent->attributes.front().attribute == kMaxHealth775);
    }
}

TEST_CASE("every version the gate admits has an unbroken road of links") {
    // A gap would leave a client let in to a chain that cannot carry it, which is worse than being
    // turned away at the door.
    for (const std::int32_t version : java::javaTranslators::supported()) {
        if (version == protocolVersions::kNative) {
            continue;
        }
        CAPTURE(version);
        CHECK(java::javaTranslators::chainFor(version) != nullptr);
    }
    CHECK(java::javaTranslators::supported().contains(protocolVersions::kV1_8));
}
