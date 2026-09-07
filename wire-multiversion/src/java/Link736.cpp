#include <basalt/wire/multiversion/java/Link736.hpp>
#include <basalt/wire/protocol/ProtocolBuffer.hpp>
#include <basalt/wire/protocol/VarInt.hpp>
#include <basalt/wire/protocol/play/PlayPackets.hpp>
#include <basalt/wire/protocol/play/PlayerStatePackets.hpp>
#include <basalt/wire/protocol/play/WorldPackets.hpp>

#include <algorithm>
#include <cstdint>

namespace basalt::wire::multiversion::java {
namespace {

namespace play = protocol::play;
using protocol::ProtocolState;

constexpr std::string_view kDimensionRegistry = "minecraft:dimension_type";
constexpr std::string_view kBiomeRegistry = "minecraft:worldgen/biome";
constexpr std::string_view kOverworld = "minecraft:overworld";
constexpr std::string_view kFallbackBiome = "minecraft:plains";

/// parity: `GameType` packed hardcore into the high bit before 1.16.2 gave it a field.
constexpr std::int32_t kHardcore = 0x08;
constexpr std::int32_t kMaxPlayersShown = 255;
constexpr std::int32_t kBiomesInChunk = 1024;
constexpr std::int32_t kSectionHeight = 16;

constexpr std::int32_t kTitle = 0x4F;
constexpr std::int32_t kSetActionBar = 2;
constexpr std::uint8_t kAboveHotbar = 2;
constexpr std::int32_t kMaxText = 262144;

[[nodiscard]] std::vector<nbt::NbtCompound> registryEntries(const nbt::NbtCompound& codec, std::string_view registry) {
    std::vector<nbt::NbtCompound> entries;
    const nbt::NbtCompound* holder = codec.getCompound(registry);
    if (holder == nullptr) {
        return entries;
    }
    const nbt::NbtList* values = holder->getList("value");
    if (values == nullptr) {
        return entries;
    }
    for (const nbt::NbtTag& value : *values) {
        if (const nbt::NbtCompound* entry = value.get<nbt::NbtCompound>()) {
            entries.push_back(*entry);
        }
    }
    return entries;
}

/// Everything 1.16.1 is told about the world: the dimensions, and nothing else.
[[nodiscard]] nbt::NbtCompound dimensionRegistry(const nbt::NbtCompound& codec) {
    nbt::NbtList dimensions;
    for (const nbt::NbtCompound& entry : registryEntries(codec, kDimensionRegistry)) {
        nbt::NbtCompound element;
        if (const nbt::NbtCompound* source = entry.getCompound("element")) {
            element = *source;
        }
        element.putString("name", std::string{entry.getString("name")});
        // 1.16.2 turned the nether's halved coordinates into a scale; before it, a flag.
        element.putBoolean("shrunk", element.getDouble("coordinate_scale", 1.0) != 1.0);
        dimensions.push(nbt::NbtTag::ofCompound(std::move(element)));
    }
    nbt::NbtCompound registry;
    registry.set("dimension", nbt::NbtTag::ofList(std::move(dimensions)));
    return registry;
}

} // namespace

bool Link736::clientbound(ProtocolState state, MutablePacket& packet) {
    if (state != ProtocolState::Play) {
        return true;
    }
    switch (packet.nativeId()) {
    case play::LoginPlayPacket::kId:
        rewriteLogin(packet);
        break;
    case play::RespawnPacket::kId:
        rewriteRespawn(packet);
        break;
    case play::SystemChatPacket::kId:
        return aboveTheHotbar(packet);
    case play::MapChunkPacket::kId:
        rewriteChunk(packet);
        break;
    case play::SectionBlocksUpdatePacket::kId:
        rewriteSectionUpdate(packet);
        break;
    default:
        break;
    }
    return true;
}

void Link736::rewriteLogin(MutablePacket& packet) {
    io::ByteBuffer input = io::ByteBuffer::wrap(packet.body);
    const std::int32_t entityId = input.readInt();
    const bool hardcore = input.readByte() != 0;
    const std::int32_t gamemode = input.readByte();
    const std::uint8_t previousGamemode = input.readByte();
    const std::int32_t worlds = protocol::varInt::read(input);
    std::vector<std::string> worldNames;
    for (std::int32_t i = 0; i < worlds && !input.failed(); ++i) {
        worldNames.push_back(protocol::readString(input));
    }
    const nbt::NbtTag codecTag = protocol::readAnonymousNbt(input);
    nbt::NbtCompound codec;
    if (const nbt::NbtCompound* parsed = codecTag.get<nbt::NbtCompound>()) {
        codec = *parsed;
    }
    const nbt::NbtTag dimension = protocol::readAnonymousNbt(input);
    const std::string worldName = protocol::readString(input);
    const std::int64_t hashedSeed = input.readLong();
    const std::int32_t maxPlayers = protocol::varInt::read(input);
    if (input.failed()) {
        return;
    }
    rememberBiomes(codec);
    known_ = registryEntries(codec, kDimensionRegistry);
    encode(packet, [&](io::ByteBuffer& output) {
        output.writeInt(entityId);
        output.writeByte(static_cast<std::uint8_t>(hardcore ? gamemode | kHardcore : gamemode));
        output.writeByte(previousGamemode);
        protocol::varInt::write(output, static_cast<std::int32_t>(worldNames.size()));
        for (const std::string& name : worldNames) {
            protocol::writeString(output, name);
        }
        protocol::writeAnonymousNbt(output, nbt::NbtTag::ofCompound(dimensionRegistry(codec)));
        protocol::writeString(output, dimensionName(&codec, dimension));
        protocol::writeString(output, worldName);
        output.writeLong(hashedSeed);
        output.writeByte(static_cast<std::uint8_t>(std::min(maxPlayers, kMaxPlayersShown)));
        output.writeBytes(input.readable());
    });
}

void Link736::rewriteRespawn(MutablePacket& packet) const {
    io::ByteBuffer input = io::ByteBuffer::wrap(packet.body);
    const nbt::NbtTag dimension = protocol::readAnonymousNbt(input);
    encode(packet, [&](io::ByteBuffer& output) {
        protocol::writeString(output, dimensionName(nullptr, dimension));
        output.writeBytes(input.readable());
    });
}

bool Link736::aboveTheHotbar(MutablePacket& packet) {
    io::ByteBuffer input = io::ByteBuffer::wrap(packet.body);
    const std::string json = protocol::readString(input, kMaxText);
    if (input.readByte() != kAboveHotbar) {
        return true;
    }
    packet.overrideId(kTitle);
    encode(packet, [&](io::ByteBuffer& output) {
        protocol::varInt::write(output, kSetActionBar);
        protocol::writeString(output, json);
    });
    return true;
}

void Link736::rewriteChunk(MutablePacket& packet) const {
    io::ByteBuffer input = io::ByteBuffer::wrap(packet.body);
    const std::int32_t x = input.readInt();
    const std::int32_t z = input.readInt();
    const std::uint8_t fullChunk = input.readByte();
    const std::int32_t mask = protocol::varInt::read(input);
    const nbt::NbtTag heightmaps = protocol::readAnonymousNbt(input);
    const std::int32_t count = protocol::varInt::read(input);
    std::vector<std::int32_t> biomeIds;
    biomeIds.reserve(static_cast<std::size_t>(count < 0 ? 0 : count));
    for (std::int32_t i = 0; i < count && !input.failed(); ++i) {
        biomeIds.push_back(protocol::varInt::read(input));
    }
    encode(packet, [&](io::ByteBuffer& output) {
        output.writeInt(x);
        output.writeInt(z);
        output.writeByte(fullChunk);
        protocol::writeBoolean(output, true);
        protocol::varInt::write(output, mask);
        protocol::writeAnonymousNbt(output, heightmaps);
        for (std::int32_t index = 0; index < kBiomesInChunk; ++index) {
            output.writeInt(biomeAt(biomeIds, static_cast<std::size_t>(index)));
        }
        output.writeBytes(input.readable());
    });
}

void Link736::rewriteSectionUpdate(MutablePacket& packet) {
    io::ByteBuffer input = io::ByteBuffer::wrap(packet.body);
    const std::int64_t packed = input.readLong();
    static_cast<void>(input.readByte());
    const std::int32_t count = protocol::varInt::read(input);
    std::vector<std::int64_t> changes;
    changes.reserve(static_cast<std::size_t>(count < 0 ? 0 : count));
    for (std::int32_t i = 0; i < count && !input.failed(); ++i) {
        changes.push_back(protocol::varLong::read(input));
    }
    const auto sectionX = static_cast<std::int32_t>(packed >> 42);
    const auto sectionY = static_cast<std::int32_t>((packed << 44) >> 44);
    const auto sectionZ = static_cast<std::int32_t>((packed << 22) >> 42);
    encode(packet, [&](io::ByteBuffer& output) {
        output.writeInt(sectionX);
        output.writeInt(sectionZ);
        protocol::varInt::write(output, static_cast<std::int32_t>(changes.size()));
        for (const std::int64_t change : changes) {
            const auto position = static_cast<std::int32_t>(change & 0xFFF);
            output.writeByte(static_cast<std::uint8_t>((((position >> 8) & 15) << 4) | ((position >> 4) & 15)));
            output.writeByte(static_cast<std::uint8_t>(sectionY * kSectionHeight + (position & 15)));
            protocol::varInt::write(output, static_cast<std::int32_t>(static_cast<std::uint64_t>(change) >> 12));
        }
    });
}

void Link736::rememberBiomes(const nbt::NbtCompound& codec) {
    const std::vector<nbt::NbtCompound> entries = registryEntries(codec, kBiomeRegistry);
    if (entries.empty()) {
        return;
    }
    std::int32_t highest = 0;
    for (const nbt::NbtCompound& entry : entries) {
        highest = std::max(highest, entry.getInt("id"));
    }
    const std::int32_t fallback = target_->staticBiomes().id(kFallbackBiome);
    biomes_.assign(static_cast<std::size_t>(highest) + 1, fallback == NameIdMap::kNone ? 0 : fallback);
    for (const nbt::NbtCompound& entry : entries) {
        const std::int32_t id = entry.getInt("id");
        if (id < 0 || static_cast<std::size_t>(id) >= biomes_.size()) {
            continue;
        }
        const std::int32_t theirs = target_->staticBiomes().id(entry.getString("name"));
        if (theirs != NameIdMap::kNone) {
            biomes_[static_cast<std::size_t>(id)] = theirs;
        }
    }
}

std::int32_t Link736::biomeAt(std::span<const std::int32_t> ids, std::size_t index) const {
    const std::int32_t id = index < ids.size() ? ids[index] : 0;
    if (id < 0 || static_cast<std::size_t>(id) >= biomes_.size()) {
        return 0;
    }
    return biomes_[static_cast<std::size_t>(id)];
}

std::string Link736::dimensionName(const nbt::NbtCompound* codec, const nbt::NbtTag& dimension) const {
    const std::vector<nbt::NbtCompound> entries =
        codec == nullptr ? known_ : registryEntries(*codec, kDimensionRegistry);
    for (const nbt::NbtCompound& entry : entries) {
        const nbt::NbtTag* element = entry.find("element");
        if (element != nullptr && *element == dimension) {
            return std::string{entry.getString("name")};
        }
    }
    return std::string{kOverworld};
}

} // namespace basalt::wire::multiversion::java
