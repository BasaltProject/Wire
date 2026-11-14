#include <basalt/wire/multiversion/java/LegacyStacks.hpp>
#include <basalt/wire/multiversion/java/Link404.hpp>
#include <basalt/wire/multiversion/java/MerchantShapes.hpp>
#include <basalt/wire/multiversion/java/NamedBlockEntities.hpp>
#include <basalt/wire/multiversion/java/NamedNbt.hpp>
#include <basalt/wire/protocol/ProtocolBuffer.hpp>
#include <basalt/wire/protocol/VarInt.hpp>
#include <basalt/wire/protocol/play/BlockEntityPackets.hpp>
#include <basalt/wire/protocol/play/EntityData.hpp>
#include <basalt/wire/protocol/play/EntityPackets.hpp>
#include <basalt/wire/protocol/play/InventoryPackets.hpp>
#include <basalt/wire/protocol/play/PlayPackets.hpp>
#include <basalt/wire/protocol/play/PlayerStatePackets.hpp>
#include <basalt/wire/protocol/play/SimplePackets.hpp>
#include <basalt/wire/protocol/play/WorldPackets.hpp>

#include <algorithm>
#include <array>
#include <bit>
#include <cstdint>
#include <string>
#include <string_view>

namespace basalt::wire::multiversion::java {
namespace {

namespace play = protocol::play;

constexpr std::size_t kCursorBytes = 3 * sizeof(float);
constexpr std::size_t kUuidBytes = 16;

constexpr std::int32_t kAddEntity = 0x00;
constexpr std::int32_t kSections = 16;
constexpr int kMaxIndirectBits = 8;
constexpr std::int32_t kMapChunk = 0x22;
constexpr std::int32_t kCustomPayload = 0x19;
constexpr std::int32_t kOverworld = 0;

constexpr std::uint8_t kNormal = 2;

constexpr std::array<std::uint8_t, 2048> kDark{};

void copyContainer(io::ByteBuffer& input, io::ByteBuffer& output) {
    const int bits = input.readByte();
    output.writeByte(static_cast<std::uint8_t>(bits));
    if (bits <= kMaxIndirectBits) {
        const std::int32_t entries = protocol::varInt::read(input);
        protocol::varInt::write(output, entries);
        for (std::int32_t i = 0; i < entries && !input.failed(); ++i) {
            protocol::varInt::write(output, protocol::varInt::read(input));
        }
    }
    const std::int32_t longs = protocol::varInt::read(input);
    protocol::varInt::write(output, longs);
    for (std::int32_t i = 0; i < longs && !input.failed(); ++i) {
        output.writeLong(input.readLong());
    }
}

[[nodiscard]] std::vector<std::vector<std::uint8_t>> readArrays(io::ByteBuffer& input, int count) {
    std::vector<std::vector<std::uint8_t>> arrays;
    arrays.reserve(std::min(static_cast<std::size_t>(count < 0 ? 0 : count), input.readableBytes()));
    for (int i = 0; i < count && !input.failed(); ++i) {
        const std::int32_t length = protocol::varInt::read(input);
        arrays.push_back(input.readBytes(static_cast<std::size_t>(length < 0 ? 0 : length)));
    }
    return arrays;
}

enum class Lead : std::uint8_t { kNone, kVarInt, kInt };

[[nodiscard]] std::int64_t toBefore1_14(std::int64_t packed) {
    const play::BlockPos pos = play::BlockPos::unpack(packed);
    return namedBlockEntities::packBefore1_14(pos.x, pos.y, pos.z);
}

[[nodiscard]] std::int64_t fromBefore1_14(std::int64_t packed) {
    const auto bits = static_cast<std::uint64_t>(packed);
    const auto field = [bits](int left, int right) {
        return static_cast<std::int32_t>(static_cast<std::int64_t>(bits << left) >> right);
    };
    return play::BlockPos{field(0, 38), field(26, 52), field(38, 38)}.pack();
}

/// 1.14 moved y to the low bits of a packed position; everything older keeps it between x and z.
void repackPosition(MutablePacket& packet, Lead lead, bool toOlder) {
    io::ByteBuffer input = io::ByteBuffer::wrap(packet.body);
    io::ByteBuffer output;
    if (lead == Lead::kVarInt) {
        protocol::varInt::write(output, protocol::varInt::read(input));
    } else if (lead == Lead::kInt) {
        output.writeInt(input.readInt());
    }
    const std::int64_t packed = input.readLong();
    output.writeLong(toOlder ? toBefore1_14(packed) : fromBefore1_14(packed));
    output.writeBytes(input.readable());
    if (!input.failed()) {
        packet.body = output.toVector();
    }
}

/// A painting's position follows its id, uuid and motive.
void repackPaintingPosition(MutablePacket& packet) {
    io::ByteBuffer input = io::ByteBuffer::wrap(packet.body);
    io::ByteBuffer output;
    protocol::varInt::write(output, protocol::varInt::read(input));
    output.writeBytes(input.readSpan(kUuidBytes));
    protocol::varInt::write(output, protocol::varInt::read(input));
    output.writeLong(toBefore1_14(input.readLong()));
    output.writeBytes(input.readable());
    if (!input.failed()) {
        packet.body = output.toVector();
    }
}

struct LegacyScreen {
    std::string_view type;
    std::uint8_t slots;
};

// parity: the window type names before 1.14, by the 1.14 menu numbering; the empty ones had no window of their own.
constexpr std::array<LegacyScreen, 23> kLegacyScreens = {{
    {"minecraft:chest", 9},
    {"minecraft:chest", 18},
    {"minecraft:chest", 27},
    {"minecraft:chest", 36},
    {"minecraft:chest", 45},
    {"minecraft:chest", 54},
    {"minecraft:dispenser", 9},
    {"minecraft:anvil", 0},
    {"minecraft:beacon", 1},
    {"", 0},
    {"minecraft:brewing_stand", 5},
    {"minecraft:crafting_table", 0},
    {"minecraft:enchanting_table", 0},
    {"minecraft:furnace", 3},
    {"", 0},
    {"minecraft:hopper", 5},
    {"", 0},
    {"", 0},
    {"minecraft:villager", 3},
    {"minecraft:shulker_box", 27},
    {"", 0},
    {"", 0},
    {"", 0},
}};

/// Before 1.14 a window was opened by type name, with its slot count beside the title.
bool openLegacyScreen(MutablePacket& packet) {
    io::ByteBuffer input = io::ByteBuffer::wrap(packet.body);
    const std::int32_t container = protocol::varInt::read(input);
    const std::int32_t menu = protocol::varInt::read(input);
    if (input.failed() || menu < 0 || static_cast<std::size_t>(menu) >= kLegacyScreens.size() ||
        kLegacyScreens[static_cast<std::size_t>(menu)].type.empty()) {
        return false;
    }
    const LegacyScreen& screen = kLegacyScreens[static_cast<std::size_t>(menu)];
    const std::string title = protocol::readString(input);
    io::ByteBuffer output;
    output.writeByte(static_cast<std::uint8_t>(container));
    protocol::writeString(output, std::string{screen.type});
    protocol::writeString(output, title);
    output.writeByte(screen.slots);
    packet.body = output.toVector();
    return !input.failed();
}

/// Before 1.14 the trades travelled on a plugin channel, without the pricing and the merchant's level.
bool toTraderList(MutablePacket& packet) {
    io::ByteBuffer input = io::ByteBuffer::wrap(packet.body);
    io::ByteBuffer output;
    protocol::writeString(output, merchantShapes::kTraderList);
    if (!merchantShapes::reshape(input, output, merchantShapes::k1_14, merchantShapes::k1_13,
                                 &legacyStacks::copyNamed)) {
        return false;
    }
    packet.body = output.toVector();
    packet.becomes(play::CustomPayloadPacket::kClientboundId);
    packet.overrideId(kCustomPayload);
    return true;
}

} // namespace

bool Link404::serverbound(protocol::ProtocolState state, MutablePacket& packet) {
    if (state != protocol::ProtocolState::Play) {
        return true;
    }
    switch (packet.nativeId()) {
    case play::ServerboundPlayerActionPacket::kId:
        repackPosition(packet, Lead::kVarInt, false);
        break;
    case play::SignUpdatePacket::kId:
    case play::SetCommandBlockPacket::kId:
    case play::SetStructureBlockPacket::kId:
        repackPosition(packet, Lead::kNone, false);
        break;
    case play::ServerboundUseItemOnPacket::kId: {
        // 1.14 put the hand first and added whether the click was inside the block.
        io::ByteBuffer input = io::ByteBuffer::wrap(packet.body);
        const std::int64_t packed = input.readLong();
        const std::int32_t face = protocol::varInt::read(input);
        const std::int32_t hand = protocol::varInt::read(input);
        const std::span<const std::uint8_t> cursor = input.readSpan(kCursorBytes);
        if (input.failed()) {
            return false;
        }
        encode(packet, [&](io::ByteBuffer& output) {
            protocol::varInt::write(output, hand);
            output.writeLong(fromBefore1_14(packed));
            protocol::varInt::write(output, face);
            output.writeBytes(cursor);
            protocol::writeBoolean(output, false);
        });
        break;
    }
    default:
        break;
    }
    return true;
}

bool Link404::clientbound(protocol::ProtocolState state, MutablePacket& packet) {
    if (state != protocol::ProtocolState::Play) {
        return true;
    }
    switch (packet.nativeId()) {
    case play::MapChunkPacket::kId:
        holdChunk(packet);
        return false;
    case play::UpdateLightPacket::kId:
        return lightTheChunk(packet);
    case play::LoginPlayPacket::kId:
        rewriteLogin(packet);
        break;
    case play::RespawnPacket::kId:
        rewriteRespawn(packet);
        break;
    case play::ChangeDifficultyPacket::kId: {
        io::ByteBuffer input = io::ByteBuffer::wrap(packet.body);
        const std::uint8_t difficulty = input.readByte();
        encode(packet, [&](io::ByteBuffer& out) { out.writeByte(difficulty); });
        break;
    }
    case play::BlockEntityDataPacket::kId:
    case play::ClientboundBlockUpdatePacket::kId:
    case play::BlockEventPacket::kId:
    case play::OpenSignEditorPacket::kId:
    case play::SpawnPositionPacket::kId:
        repackPosition(packet, Lead::kNone, true);
        break;
    case play::OpenScreenPacket::kId:
        return openLegacyScreen(packet);
    case play::MerchantOffersPacket::kId:
        return toTraderList(packet);
    case play::BlockDestructionPacket::kId:
        repackPosition(packet, Lead::kVarInt, true);
        break;
    case play::LevelEventPacket::kId:
        repackPosition(packet, Lead::kInt, true);
        break;
    case play::SpawnEntityPacket::kId:
        if (packet.spawn == MutablePacket::Spawn::kPainting) {
            repackPaintingPosition(packet);
        } else if (packet.id == kAddEntity) {
            narrowType(packet);
        }
        break;
    default:
        break;
    }
    return true;
}

void Link404::holdChunk(MutablePacket& packet) {
    io::ByteBuffer input = io::ByteBuffer::wrap(packet.body);
    PendingChunk held;
    held.x = input.readInt();
    held.z = input.readInt();
    held.fullChunk = input.readByte();
    held.mask = protocol::varInt::read(input);
    static_cast<void>(readNamedTag(input));
    const std::int32_t length = protocol::varInt::read(input);
    held.sections = input.readBytes(static_cast<std::size_t>(length < 0 ? 0 : length));
    const std::span<const std::uint8_t> rest = input.readable();
    held.rest.assign(rest.begin(), rest.end());
    if (input.failed()) {
        return;
    }
    pending_ = std::move(held);
}

bool Link404::lightTheChunk(MutablePacket& packet) {
    if (!pending_.has_value()) {
        return false;
    }
    const PendingChunk held = *std::move(pending_);
    pending_.reset();
    io::ByteBuffer input = io::ByteBuffer::wrap(packet.body);
    const std::int32_t x = protocol::varInt::read(input);
    const std::int32_t z = protocol::varInt::read(input);
    if (x != held.x || z != held.z) {
        return false;
    }
    const std::int32_t skyMask = protocol::varInt::read(input);
    const std::int32_t blockMask = protocol::varInt::read(input);
    static_cast<void>(protocol::varInt::read(input));
    static_cast<void>(protocol::varInt::read(input));
    const std::vector<std::vector<std::uint8_t>> skyLight =
        readArrays(input, std::popcount(static_cast<std::uint32_t>(skyMask)));
    const std::vector<std::vector<std::uint8_t>> blockLight =
        readArrays(input, std::popcount(static_cast<std::uint32_t>(blockMask)));

    io::ByteBuffer sections = io::ByteBuffer::wrap(held.sections);
    io::ByteBuffer data;
    std::size_t sky = 0;
    std::size_t block = 0;
    for (std::int32_t index = 0; index < kSections; ++index) {
        const std::vector<std::uint8_t>* skyHere = nullptr;
        const std::vector<std::uint8_t>* blockHere = nullptr;
        if ((skyMask & (1 << (index + 1))) != 0 && sky < skyLight.size()) {
            skyHere = &skyLight[sky++];
        }
        if ((blockMask & (1 << (index + 1))) != 0 && block < blockLight.size()) {
            blockHere = &blockLight[block++];
        }
        if ((held.mask & (1 << index)) == 0) {
            continue;
        }
        static_cast<void>(sections.readShort());
        copyContainer(sections, data);
        data.writeBytes(blockHere == nullptr ? std::span<const std::uint8_t>{kDark} : std::span{*blockHere});
        if (hasSkyLight_) {
            data.writeBytes(skyHere == nullptr ? std::span<const std::uint8_t>{kDark} : std::span{*skyHere});
        }
    }
    data.writeBytes(sections.readable());

    const std::vector<std::uint8_t> body = data.toVector();
    // From here down it is a chunk, not the light it arrived as: the links below have to know.
    packet.becomes(play::MapChunkPacket::kId);
    packet.overrideId(kMapChunk);
    encode(packet, [&](io::ByteBuffer& output) {
        output.writeInt(held.x);
        output.writeInt(held.z);
        output.writeByte(held.fullChunk);
        protocol::varInt::write(output, held.mask);
        protocol::varInt::write(output, static_cast<std::int32_t>(body.size()));
        output.writeBytes(body);
        output.writeBytes(held.rest);
    });
    return true;
}

void Link404::rewriteLogin(MutablePacket& packet) {
    io::ByteBuffer input = io::ByteBuffer::wrap(packet.body);
    const std::int32_t entityId = input.readInt();
    const std::uint8_t gamemode = input.readByte();
    const std::int32_t dimension = input.readInt();
    const std::uint8_t maxPlayers = input.readByte();
    const std::string levelType = protocol::readString(input);
    static_cast<void>(protocol::varInt::read(input));
    const std::uint8_t reducedDebugInfo = input.readByte();
    if (input.failed()) {
        return;
    }
    hasSkyLight_ = dimension == kOverworld;
    encode(packet, [&](io::ByteBuffer& output) {
        output.writeInt(entityId);
        output.writeByte(gamemode);
        output.writeInt(dimension);
        output.writeByte(kNormal);
        output.writeByte(maxPlayers);
        protocol::writeString(output, levelType);
        output.writeByte(reducedDebugInfo);
    });
}

void Link404::rewriteRespawn(MutablePacket& packet) {
    io::ByteBuffer input = io::ByteBuffer::wrap(packet.body);
    const std::int32_t dimension = input.readInt();
    const std::uint8_t gamemode = input.readByte();
    const std::string levelType = protocol::readString(input);
    if (input.failed()) {
        return;
    }
    hasSkyLight_ = dimension == kOverworld;
    encode(packet, [&](io::ByteBuffer& output) {
        output.writeInt(dimension);
        output.writeByte(kNormal);
        output.writeByte(gamemode);
        protocol::writeString(output, levelType);
    });
}

void Link404::narrowType(MutablePacket& packet) {
    io::ByteBuffer input = io::ByteBuffer::wrap(packet.body);
    const std::int32_t entityId = protocol::varInt::read(input);
    const std::int64_t mostSignificant = input.readLong();
    const std::int64_t leastSignificant = input.readLong();
    const std::int32_t type = protocol::varInt::read(input);
    encode(packet, [&](io::ByteBuffer& output) {
        protocol::varInt::write(output, entityId);
        output.writeLong(mostSignificant);
        output.writeLong(leastSignificant);
        output.writeByte(static_cast<std::uint8_t>(type));
        output.writeBytes(input.readable());
    });
}

} // namespace basalt::wire::multiversion::java
