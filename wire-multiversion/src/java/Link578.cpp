#include <basalt/wire/multiversion/CompactBitStorage.hpp>
#include <basalt/wire/multiversion/java/Link578.hpp>
#include <basalt/wire/protocol/ProtocolBuffer.hpp>
#include <basalt/wire/protocol/VarInt.hpp>
#include <basalt/wire/protocol/play/BitStorage.hpp>
#include <basalt/wire/protocol/play/EntityPackets.hpp>
#include <basalt/wire/protocol/play/PlayPackets.hpp>
#include <basalt/wire/protocol/play/PlayerStatePackets.hpp>
#include <basalt/wire/protocol/play/WorldPackets.hpp>

#include <algorithm>
#include <array>
#include <cstdint>
#include <utility>

namespace basalt::wire::multiversion::java {
namespace {

namespace play = protocol::play;
using protocol::ProtocolState;

constexpr std::string_view kNether = "minecraft:the_nether";
constexpr std::string_view kTheEnd = "minecraft:the_end";
constexpr std::int32_t kMaxText = 262144;

constexpr std::int32_t kSections = 16;
constexpr std::int32_t kBiomesInChunk = 1024;
constexpr std::size_t kBlocksPerSection = 16 * 16 * 16;
constexpr int kBlockMaxIndirectBits = 8;
constexpr int kHeightmapBits = 9;
constexpr std::size_t kHeightmapEntries = 256;

constexpr std::int32_t kSlotMask = 0x7F;
constexpr std::int32_t kMoreSlots = 0x80;

constexpr std::size_t kModifierBytes = 16 + 8 + 1;

/// parity: the keys of `Attributes` before 1.16 gave them a namespace and snake case.
constexpr std::array<std::pair<std::string_view, std::string_view>, 13> kLegacyNames = {{
    {"minecraft:generic.max_health", "generic.maxHealth"},
    {"minecraft:generic.follow_range", "generic.followRange"},
    {"minecraft:generic.knockback_resistance", "generic.knockbackResistance"},
    {"minecraft:generic.movement_speed", "generic.movementSpeed"},
    {"minecraft:generic.flying_speed", "generic.flyingSpeed"},
    {"minecraft:generic.attack_damage", "generic.attackDamage"},
    {"minecraft:generic.attack_knockback", "generic.attackKnockback"},
    {"minecraft:generic.attack_speed", "generic.attackSpeed"},
    {"minecraft:generic.armor", "generic.armor"},
    {"minecraft:generic.armor_toughness", "generic.armorToughness"},
    {"minecraft:generic.luck", "generic.luck"},
    {"minecraft:horse.jump_strength", "horse.jumpStrength"},
    {"minecraft:zombie.spawn_reinforcements", "zombie.spawnReinforcements"},
}};

[[nodiscard]] std::string_view legacyName(std::string_view name) {
    for (const auto& [modern, legacy] : kLegacyNames) {
        if (modern == name) {
            return legacy;
        }
    }
    return name;
}

[[nodiscard]] std::int32_t dimensionNumber(std::string_view name) {
    if (name == kNether) {
        return -1;
    }
    if (name == kTheEnd) {
        return 1;
    }
    return 0;
}

[[nodiscard]] std::string_view levelType(bool flat) { return flat ? "flat" : "default"; }

void repackContainer(io::ByteBuffer& input, io::ByteBuffer& output) {
    const int bits = input.readByte();
    output.writeByte(static_cast<std::uint8_t>(bits));
    if (bits <= kBlockMaxIndirectBits) {
        const std::int32_t entries = protocol::varInt::read(input);
        protocol::varInt::write(output, entries);
        for (std::int32_t i = 0; i < entries && !input.failed(); ++i) {
            protocol::varInt::write(output, protocol::varInt::read(input));
        }
    }
    const std::int32_t count = protocol::varInt::read(input);
    std::vector<std::uint64_t> words;
    words.reserve(std::min(static_cast<std::size_t>(count < 0 ? 0 : count), input.readableBytes()));
    for (std::int32_t i = 0; i < count && !input.failed(); ++i) {
        words.push_back(static_cast<std::uint64_t>(input.readLong()));
    }
    if (bits < 0 || bits > 32 || words.size() != play::bitStorageLongCount(bits, kBlocksPerSection)) {
        input.markFailed();
        return;
    }
    const play::BitStorage storage{bits, kBlocksPerSection, std::move(words)};
    std::vector<std::int32_t> values;
    values.reserve(kBlocksPerSection);
    for (std::size_t i = 0; i < kBlocksPerSection; ++i) {
        values.push_back(storage.get(i));
    }
    const std::vector<std::int64_t> compact =
        bits == 0 ? std::vector<std::int64_t>{} : compactBitStorage::pack(values, bits);
    protocol::varInt::write(output, static_cast<std::int32_t>(compact.size()));
    for (const std::int64_t word : compact) {
        output.writeLong(word);
    }
}

[[nodiscard]] nbt::NbtTag compactHeightmaps(const nbt::NbtTag& heightmaps) {
    const nbt::NbtCompound* compound = heightmaps.get<nbt::NbtCompound>();
    if (compound == nullptr) {
        return heightmaps;
    }
    nbt::NbtCompound rewritten;
    for (std::size_t i = 0; i < compound->size(); ++i) {
        const std::string& name = compound->names()[i];
        const auto* data = compound->values()[i].get<std::vector<std::int64_t>>();
        if (data == nullptr) {
            rewritten.set(name, compound->values()[i]);
            continue;
        }
        if (data->size() != play::bitStorageLongCount(kHeightmapBits, kHeightmapEntries)) {
            continue;
        }
        std::vector<std::uint64_t> words;
        words.reserve(data->size());
        for (const std::int64_t word : *data) {
            words.push_back(static_cast<std::uint64_t>(word));
        }
        const play::BitStorage storage{kHeightmapBits, kHeightmapEntries, std::move(words)};
        std::vector<std::int32_t> values;
        values.reserve(kHeightmapEntries);
        for (std::size_t entry = 0; entry < kHeightmapEntries; ++entry) {
            values.push_back(storage.get(entry));
        }
        rewritten.putLongArray(name, compactBitStorage::pack(values, kHeightmapBits));
    }
    return nbt::NbtTag::ofCompound(std::move(rewritten));
}

[[nodiscard]] std::vector<std::uint8_t> itemBytes(io::ByteBuffer& input) {
    io::ByteBuffer output;
    const std::uint8_t present = input.readByte();
    output.writeByte(present);
    if (present == 0) {
        return output.toVector();
    }
    protocol::varInt::write(output, protocol::varInt::read(input));
    output.writeByte(input.readByte());
    protocol::writeAnonymousNbt(output, protocol::readAnonymousNbt(input));
    return output.toVector();
}

} // namespace

bool Link578::clientbound(ProtocolState state, MutablePacket& packet) {
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
        dropSender(packet);
        break;
    case play::MapChunkPacket::kId:
        repackChunk(packet);
        break;
    case play::UpdateLightPacket::kId:
        dropTrustEdges(packet);
        break;
    case play::SetEquipmentPacket::kId:
        splitEquipment(packet);
        break;
    case play::UpdateAttributesPacket::kId:
        renameAttributes(packet);
        break;
    default:
        break;
    }
    return true;
}

bool Link578::serverbound(ProtocolState state, MutablePacket& packet) {
    if (state != ProtocolState::Play) {
        return true;
    }
    switch (packet.nativeId()) {
    case play::InteractPacket::kId:
        encode(packet, [&](io::ByteBuffer& output) {
            output.writeBytes(packet.body);
            output.writeByte(0);
        });
        break;
    case play::ServerboundPlayerAbilitiesPacket::kId: {
        io::ByteBuffer input = io::ByteBuffer::wrap(packet.body);
        const std::uint8_t flags = input.readByte();
        encode(packet, [&](io::ByteBuffer& out) { out.writeByte(flags); });
        break;
    }
    default:
        break;
    }
    return true;
}

void Link578::rewriteLogin(MutablePacket& packet) {
    io::ByteBuffer input = io::ByteBuffer::wrap(packet.body);
    const std::int32_t entityId = input.readInt();
    const std::uint8_t gamemode = input.readByte();
    static_cast<void>(input.readByte());
    const std::int32_t worlds = protocol::varInt::read(input);
    for (std::int32_t i = 0; i < worlds && !input.failed(); ++i) {
        static_cast<void>(protocol::readString(input));
    }
    static_cast<void>(protocol::readAnonymousNbt(input));
    const std::string dimension = protocol::readString(input);
    static_cast<void>(protocol::readString(input));
    const std::int64_t hashedSeed = input.readLong();
    const std::uint8_t maxPlayers = input.readByte();
    const std::int32_t viewDistance = protocol::varInt::read(input);
    const std::uint8_t reducedDebugInfo = input.readByte();
    const std::uint8_t respawnScreen = input.readByte();
    static_cast<void>(input.readByte());
    const bool flat = input.readByte() != 0;
    if (input.failed()) {
        return;
    }
    encode(packet, [&](io::ByteBuffer& output) {
        output.writeInt(entityId);
        output.writeByte(gamemode);
        output.writeInt(dimensionNumber(dimension));
        output.writeLong(hashedSeed);
        output.writeByte(maxPlayers);
        protocol::writeString(output, levelType(flat));
        protocol::varInt::write(output, viewDistance);
        output.writeByte(reducedDebugInfo);
        output.writeByte(respawnScreen);
    });
}

void Link578::rewriteRespawn(MutablePacket& packet) {
    io::ByteBuffer input = io::ByteBuffer::wrap(packet.body);
    const std::string dimension = protocol::readString(input);
    static_cast<void>(protocol::readString(input));
    const std::int64_t hashedSeed = input.readLong();
    const std::uint8_t gamemode = input.readByte();
    static_cast<void>(input.readByte());
    static_cast<void>(input.readByte());
    const bool flat = input.readByte() != 0;
    if (input.failed()) {
        return;
    }
    encode(packet, [&](io::ByteBuffer& output) {
        output.writeInt(dimensionNumber(dimension));
        output.writeLong(hashedSeed);
        output.writeByte(gamemode);
        protocol::writeString(output, levelType(flat));
    });
}

void Link578::dropSender(MutablePacket& packet) {
    io::ByteBuffer input = io::ByteBuffer::wrap(packet.body);
    const std::string json = protocol::readString(input, kMaxText);
    const std::uint8_t position = input.readByte();
    if (input.failed()) {
        return;
    }
    encode(packet, [&](io::ByteBuffer& output) {
        protocol::writeString(output, json);
        output.writeByte(position);
    });
}

void Link578::repackChunk(MutablePacket& packet) {
    io::ByteBuffer input = io::ByteBuffer::wrap(packet.body);
    const std::int32_t x = input.readInt();
    const std::int32_t z = input.readInt();
    const std::uint8_t fullChunk = input.readByte();
    static_cast<void>(input.readByte());
    const std::int32_t mask = protocol::varInt::read(input);
    const nbt::NbtTag heightmaps = protocol::readAnonymousNbt(input);
    std::vector<std::int32_t> biomes;
    biomes.reserve(kBiomesInChunk);
    for (std::int32_t i = 0; i < kBiomesInChunk && !input.failed(); ++i) {
        biomes.push_back(input.readInt());
    }
    const std::int32_t sectionBytes = protocol::varInt::read(input);
    io::ByteBuffer sections =
        io::ByteBuffer::wrap(input.readBytes(static_cast<std::size_t>(sectionBytes < 0 ? 0 : sectionBytes)));
    const std::vector<std::uint8_t> rest{input.readable().begin(), input.readable().end()};

    io::ByteBuffer packed;
    for (std::int32_t index = 0; index < kSections; ++index) {
        if ((mask & (1 << index)) == 0) {
            continue;
        }
        packed.writeShort(sections.readShort());
        repackContainer(sections, packed);
    }
    const std::vector<std::uint8_t> data = packed.toVector();
    encode(packet, [&](io::ByteBuffer& output) {
        output.writeInt(x);
        output.writeInt(z);
        output.writeByte(fullChunk);
        protocol::varInt::write(output, mask);
        protocol::writeAnonymousNbt(output, compactHeightmaps(heightmaps));
        for (const std::int32_t biome : biomes) {
            output.writeInt(biome);
        }
        protocol::varInt::write(output, static_cast<std::int32_t>(data.size()));
        output.writeBytes(data);
        output.writeBytes(rest);
    });
}

void Link578::dropTrustEdges(MutablePacket& packet) {
    io::ByteBuffer input = io::ByteBuffer::wrap(packet.body);
    const std::int32_t x = protocol::varInt::read(input);
    const std::int32_t z = protocol::varInt::read(input);
    static_cast<void>(input.readByte());
    encode(packet, [&](io::ByteBuffer& output) {
        protocol::varInt::write(output, x);
        protocol::varInt::write(output, z);
        output.writeBytes(input.readable());
    });
}

void Link578::splitEquipment(MutablePacket& packet) const {
    io::ByteBuffer input = io::ByteBuffer::wrap(packet.body);
    const std::int32_t entityId = protocol::varInt::read(input);
    std::vector<std::pair<std::int32_t, std::vector<std::uint8_t>>> slots;
    while (!input.failed()) {
        const std::int32_t slot = input.readByte();
        const std::size_t start = input.readableBytes();
        std::vector<std::uint8_t> item = itemBytes(input);
        if (start == input.readableBytes()) {
            break;
        }
        slots.emplace_back(slot & kSlotMask, std::move(item));
        if ((slot & kMoreSlots) == 0) {
            break;
        }
    }
    if (slots.empty()) {
        return;
    }
    encode(packet, [&](io::ByteBuffer& output) {
        protocol::varInt::write(output, entityId);
        protocol::varInt::write(output, slots.front().first);
        output.writeBytes(slots.front().second);
    });
    for (std::size_t index = 1; index < slots.size(); ++index) {
        io::ByteBuffer body;
        protocol::varInt::write(body, entityId);
        protocol::varInt::write(body, slots[index].first);
        body.writeBytes(slots[index].second);
        packet.extras.emplace_back(packets().toOlder(ProtocolState::Play, packet.id), body.toVector());
    }
}

void Link578::renameAttributes(MutablePacket& packet) {
    io::ByteBuffer input = io::ByteBuffer::wrap(packet.body);
    const std::int32_t entityId = protocol::varInt::read(input);
    const std::int32_t count = input.readInt();

    struct Entry {
        std::string name;
        double base = 0.0;
        std::vector<std::uint8_t> modifiers;
    };

    std::vector<Entry> entries;
    entries.reserve(std::min(static_cast<std::size_t>(count < 0 ? 0 : count), input.readableBytes()));
    for (std::int32_t i = 0; i < count && !input.failed(); ++i) {
        Entry entry;
        entry.name = protocol::readString(input);
        entry.base = input.readDouble();
        const std::int32_t modifiers = protocol::varInt::read(input);
        entry.modifiers = input.readBytes(static_cast<std::size_t>(modifiers < 0 ? 0 : modifiers) * kModifierBytes);
        entries.push_back(std::move(entry));
    }
    if (input.failed()) {
        return;
    }
    encode(packet, [&](io::ByteBuffer& output) {
        protocol::varInt::write(output, entityId);
        output.writeInt(count);
        for (const Entry& entry : entries) {
            protocol::writeString(output, legacyName(entry.name));
            output.writeDouble(entry.base);
            protocol::varInt::write(output, static_cast<std::int32_t>(entry.modifiers.size() / kModifierBytes));
            output.writeBytes(entry.modifiers);
        }
    });
}

} // namespace basalt::wire::multiversion::java
