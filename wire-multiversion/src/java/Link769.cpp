#include <basalt/wire/multiversion/java/Link769.hpp>
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
        const auto chunk = decode(packet, [](io::ByteBuffer& in) { return play::MapChunkPacket::read(in); });
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
    // Everything after the heightmaps is the same in both versions, so it is written once by the
    // packet itself and copied across rather than spelled out again here.
    play::MapChunkPacket modernChunk = chunk;
    modernChunk.chunkData = olderChunks_.rewrite(chunk.chunkData);
    io::ByteBuffer modern;
    modernChunk.write(modern);
    static_cast<void>(modern.readInt());
    static_cast<void>(modern.readInt());
    const std::int32_t heightmaps = protocol::varInt::read(modern);
    for (std::int32_t i = 0; i < heightmaps && !modern.failed(); ++i) {
        static_cast<void>(protocol::varInt::read(modern));
        const std::int32_t longs = protocol::varInt::read(modern);
        for (std::int32_t j = 0; j < longs && !modern.failed(); ++j) {
            static_cast<void>(modern.readLong());
        }
    }
    output.writeInt(chunk.x);
    output.writeInt(chunk.z);
    protocol::writeAnonymousNbt(output, nbt::NbtTag::ofCompound(namedHeightmaps(chunk.heightmaps)));
    output.writeBytes(modern.readable());
}

} // namespace basalt::wire::multiversion::java
