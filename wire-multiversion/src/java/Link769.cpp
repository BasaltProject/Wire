#include <basalt/wire/multiversion/java/ChatShapes.hpp>
#include <basalt/wire/multiversion/java/Link769.hpp>
#include <basalt/wire/multiversion/java/LongMaskLight.hpp>
#include <basalt/wire/multiversion/java/NamedBlockEntities.hpp>
#include <basalt/wire/protocol/ProtocolBuffer.hpp>
#include <basalt/wire/protocol/VarInt.hpp>
#include <basalt/wire/protocol/configuration/ConfigurationPackets.hpp>
#include <basalt/wire/protocol/play/BlockEntityPackets.hpp>
#include <basalt/wire/protocol/play/ChatPackets.hpp>
#include <basalt/wire/protocol/play/EntityData.hpp>
#include <basalt/wire/protocol/play/HudPackets.hpp>
#include <basalt/wire/protocol/play/InventoryPackets.hpp>
#include <basalt/wire/protocol/play/PlayPackets.hpp>
#include <basalt/wire/protocol/play/PlayerStatePackets.hpp>
#include <basalt/wire/protocol/play/SimplePackets.hpp>
#include <basalt/wire/protocol/play/UiPackets.hpp>
#include <basalt/wire/text/ComponentNbt.hpp>

#include <cstdint>

namespace basalt::wire::multiversion::java {
namespace {

namespace play = protocol::play;
using protocol::ProtocolState;

constexpr std::int32_t kAddExperienceOrb769 = 2;
constexpr std::size_t kSpawnAngleBytes = 3;

[[nodiscard]] nbt::NbtCompound namedHeightmaps(std::span<const play::Heightmap> heightmaps) {
    nbt::NbtCompound tag;
    for (const play::Heightmap& heightmap : heightmaps) {
        tag.putLongArray(play::heightmapName(heightmap.type), heightmap.data);
    }
    return tag;
}

/// 1.21.5 renamed click and hover events inside text components; a 1.21.4 client only knows the old names.
template <typename Packet>
void rewriteText(MutablePacket& packet) {
    io::ByteBuffer input = io::ByteBuffer::wrap(packet.body);
    auto value = Packet::read(input);
    if (!value.has_value()) {
        return;
    }
    io::ByteBuffer output;
    {
        const text::componentNbt::ScopedLegacyEvents legacy;
        value->write(output);
    }
    packet.body = output.toVector();
}

} // namespace

Link769::Link769(const VersionTables& tables)
    : RenumberingLink(tables), olderChunks_(IdMap::identity(), ChunkRewriter::Options{
                                                                   .olderDirectBits = tables.blockStateDirectBits,
                                                                   .lengthPrefixedData = true,
                                                                   .fluidCount = false,
                                                                   .readsFluidCount = false,
                                                               }) {}

bool Link769::clientbound(ProtocolState state, MutablePacket& packet) {
    if (state == ProtocolState::Configuration &&
        packet.nativeId() == protocol::configuration::ConfigurationDisconnectPacket::kId) {
        rewriteText<protocol::configuration::ConfigurationDisconnectPacket>(packet);
        return RenumberingLink::clientbound(state, packet);
    }
    if (state != ProtocolState::Play) {
        return RenumberingLink::clientbound(state, packet);
    }
    switch (packet.nativeId()) {
    case play::SystemChatPacket::kId:
        rewriteText<play::SystemChatPacket>(packet);
        return RenumberingLink::clientbound(state, packet);
    case play::TabListPacket::kId:
        rewriteText<play::TabListPacket>(packet);
        return RenumberingLink::clientbound(state, packet);
    case play::PlayDisconnectPacket::kId:
        rewriteText<play::PlayDisconnectPacket>(packet);
        return RenumberingLink::clientbound(state, packet);
    case play::OpenScreenPacket::kId:
        rewriteText<play::OpenScreenPacket>(packet);
        return RenumberingLink::clientbound(state, packet);
    case play::BossEventPacket::kId:
        rewriteText<play::BossEventPacket>(packet);
        return RenumberingLink::clientbound(state, packet);
    case play::SetObjectivePacket::kId:
        rewriteText<play::SetObjectivePacket>(packet);
        return RenumberingLink::clientbound(state, packet);
    case play::SetScorePacket::kId:
        rewriteText<play::SetScorePacket>(packet);
        return RenumberingLink::clientbound(state, packet);
    case play::SetPlayerTeamPacket::kId:
        rewriteText<play::SetPlayerTeamPacket>(packet);
        return RenumberingLink::clientbound(state, packet);
    case play::DisguisedChatPacket::kId:
        rewriteText<play::DisguisedChatPacket>(packet);
        return RenumberingLink::clientbound(state, packet);
    case play::PlayerChatPacket::kId: {
        rewriteText<play::PlayerChatPacket>(packet);
        // 1.21.5 numbered every chat message before its sender.
        io::ByteBuffer input = io::ByteBuffer::wrap(packet.body);
        static_cast<void>(protocol::varInt::read(input));
        if (!input.failed()) {
            const std::span<const std::uint8_t> rest = input.readable();
            packet.body.assign(rest.begin(), rest.end());
        }
        return RenumberingLink::clientbound(state, packet);
    }
    case play::PlayerCombatKillPacket::kId:
        rewriteText<play::PlayerCombatKillPacket>(packet);
        return RenumberingLink::clientbound(state, packet);
    case play::kSetTitleTextId:
    case play::kSetSubtitleTextId:
    case play::kSetActionBarTextId:
        rewriteText<play::TitleTextPacket>(packet);
        return RenumberingLink::clientbound(state, packet);
    case play::MapChunkPacket::kId: {
        const auto chunk = decode(packet, [](io::ByteBuffer& in) -> std::optional<play::MapChunkPacket> {
            auto read = play::MapChunkPacket::read(in, false);
            auto light = readLongMaskLight(in);
            if (!read.has_value() || !light.has_value()) {
                return std::nullopt;
            }
            read->light = std::move(*light);
            return read;
        });
        if (!chunk.has_value()) {
            return true;
        }
        encode(packet, [&](io::ByteBuffer& out) { writeOlderChunk(*chunk, out); });
        return true;
    }
    case play::SpawnEntityPacket::kId:
        if (packet.spawn == MutablePacket::Spawn::kExperienceOrb) {
            toExperienceOrb(packet);
        }
        return true;
    case play::BlockEntityDataPacket::kId: {
        auto data = decode(packet, [](io::ByteBuffer& in) { return play::BlockEntityDataPacket::read(in); });
        if (data.has_value() && data->tag.has_value() && namedBlockEntities::signLinesAsJson(*data->tag)) {
            encode(packet, [&](io::ByteBuffer& out) { data->write(out); });
        }
        return true;
    }
    case play::ChunksBiomesPacket::kId: {
        auto biomes = decode(packet, [](io::ByteBuffer& in) { return play::ChunksBiomesPacket::read(in); });
        if (!biomes.has_value()) {
            return true;
        }
        for (play::ChunkBiomeData& one : biomes->chunks) {
            one.biomes = olderChunks_.rewriteBiomes(one.biomes);
        }
        encode(packet, [&](io::ByteBuffer& out) { biomes->write(out); });
        return true;
    }
    default:
        return RenumberingLink::clientbound(state, packet);
    }
}

void Link769::toExperienceOrb(MutablePacket& packet) {
    // parity: `ClientboundAddExperienceOrbPacket`, which 1.21.5 folded into the spawn with the value as its data.
    io::ByteBuffer input = io::ByteBuffer::wrap(packet.body);
    const std::int32_t entityId = protocol::varInt::read(input);
    static_cast<void>(input.readLong());
    static_cast<void>(input.readLong());
    static_cast<void>(protocol::varInt::read(input));
    const double x = input.readDouble();
    const double y = input.readDouble();
    const double z = input.readDouble();
    static_cast<void>(input.readSpan(kSpawnAngleBytes));
    const std::int32_t value = protocol::varInt::read(input);
    if (input.failed()) {
        return;
    }
    packet.overrideId(kAddExperienceOrb769);
    encode(packet, [&](io::ByteBuffer& output) {
        protocol::varInt::write(output, entityId);
        output.writeDouble(x);
        output.writeDouble(y);
        output.writeDouble(z);
        output.writeShort(static_cast<std::int16_t>(value));
    });
}

void Link769::writeOlderChunk(const play::MapChunkPacket& chunk, io::ByteBuffer& output) {
    output.writeInt(chunk.x);
    output.writeInt(chunk.z);
    protocol::writeAnonymousNbt(output, nbt::NbtTag::ofCompound(namedHeightmaps(chunk.heightmaps)));
    protocol::writeByteArray(output, olderChunks_.rewrite(chunk.chunkData));
    protocol::varInt::write(output, static_cast<std::int32_t>(chunk.blockEntities.size()));
    for (play::ChunkBlockEntityInfo blockEntity : chunk.blockEntities) {
        if (blockEntity.tag.has_value()) {
            static_cast<void>(namedBlockEntities::signLinesAsJson(*blockEntity.tag));
        }
        blockEntity.write(output);
    }
    writeLongMaskLight(output, chunk.light);
}

bool Link769::serverbound(ProtocolState state, MutablePacket& packet) {
    if (state == ProtocolState::Play) {
        chatShapes::to1_21_5(packet);
    }
    if (state != ProtocolState::Play || packet.nativeId() != play::ServerboundContainerClickPacket::kId) {
        return true;
    }
    // No hashes of what the client predicted: the server answers a click with what it really holds.
    io::ByteBuffer input = io::ByteBuffer::wrap(packet.body);
    const std::int32_t container = protocol::varInt::read(input);
    const std::int32_t stateId = protocol::varInt::read(input);
    const std::int16_t slot = input.readShort();
    const std::uint8_t button = input.readByte();
    const std::int32_t mode = protocol::varInt::read(input);
    if (input.failed()) {
        return false;
    }
    encode(packet, [&](io::ByteBuffer& output) {
        protocol::varInt::write(output, container);
        protocol::varInt::write(output, stateId);
        output.writeShort(slot);
        output.writeByte(button);
        protocol::varInt::write(output, mode);
        protocol::varInt::write(output, 0);
        protocol::writeBoolean(output, false);
    });
    return true;
}

} // namespace basalt::wire::multiversion::java
