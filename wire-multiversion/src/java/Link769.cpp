#include <basalt/wire/multiversion/java/Link769.hpp>
#include <basalt/wire/multiversion/java/LongMaskLight.hpp>
#include <basalt/wire/protocol/ProtocolBuffer.hpp>
#include <basalt/wire/protocol/VarInt.hpp>
#include <basalt/wire/protocol/configuration/ConfigurationPackets.hpp>
#include <basalt/wire/protocol/play/ChatPackets.hpp>
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
    case play::PlayerChatPacket::kId:
        rewriteText<play::PlayerChatPacket>(packet);
        return RenumberingLink::clientbound(state, packet);
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

void Link769::writeOlderChunk(const play::MapChunkPacket& chunk, io::ByteBuffer& output) {
    output.writeInt(chunk.x);
    output.writeInt(chunk.z);
    protocol::writeAnonymousNbt(output, nbt::NbtTag::ofCompound(namedHeightmaps(chunk.heightmaps)));
    protocol::writeByteArray(output, olderChunks_.rewrite(chunk.chunkData));
    protocol::varInt::write(output, static_cast<std::int32_t>(chunk.blockEntities.size()));
    for (const play::ChunkBlockEntityInfo& blockEntity : chunk.blockEntities) {
        blockEntity.write(output);
    }
    writeLongMaskLight(output, chunk.light);
}

} // namespace basalt::wire::multiversion::java
