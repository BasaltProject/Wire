#include <basalt/wire/multiversion/java/Link769.hpp>
#include <basalt/wire/multiversion/java/LongMaskLight.hpp>
#include <basalt/wire/protocol/ProtocolBuffer.hpp>
#include <basalt/wire/protocol/VarInt.hpp>
#include <basalt/wire/protocol/play/SimplePackets.hpp>

#include <cstdint>

namespace basalt::wire::multiversion::java {
namespace {

namespace play = protocol::play;
using protocol::ProtocolState;

/// The heightmaps as 1.21.4 wants them: a compound keyed by the name of each one.
[[nodiscard]] nbt::NbtCompound namedHeightmaps(std::span<const play::Heightmap> heightmaps) {
    nbt::NbtCompound tag;
    for (const play::Heightmap& heightmap : heightmaps) {
        tag.putLongArray(play::heightmapName(heightmap.type), heightmap.data);
    }
    return tag;
}

} // namespace

Link769::Link769(const VersionTables& tables)
    : RenumberingLink(tables), olderChunks_(IdMap::identity(), ChunkRewriter::Options{
                                                                   .olderDirectBits = tables.blockStateDirectBits,
                                                                   .lengthPrefixedData = true,
                                                                   // The fluid count is long gone by here: 26.1 to
                                                                   // 1.21.11 took it out.
                                                                   .fluidCount = false,
                                                                   .readsFluidCount = false,
                                                               }) {}

bool Link769::clientbound(ProtocolState state, MutablePacket& packet) {
    // Not the base first here: this link replaces the chunk rewriting the base class does rather
    // than adding to it, so it reads the packet as it arrives and writes the older form itself.
    if (state != ProtocolState::Play) {
        return RenumberingLink::clientbound(state, packet);
    }
    switch (packet.nativeId()) {
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
