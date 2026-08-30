#pragma once

#include <basalt/wire/multiversion/ChunkRewriter.hpp>
#include <basalt/wire/multiversion/java/RenumberingLink.hpp>
#include <basalt/wire/nbt/NbtTag.hpp>
#include <basalt/wire/protocol/play/WorldPackets.hpp>

#include <span>

namespace basalt::wire::multiversion::java {

/// 1.21.5 to 1.21.4, where the chunk changed shape twice over.
///
/// A palette used to write how many longs it holds; from 1.21.5 it does not, because the width and
/// the size say it. And the heightmaps used to be an NBT compound keyed by name, where they are now
/// a list of numbered ones.
class Link769 final : public RenumberingLink {
public:
    explicit Link769(const VersionTables& tables);

    [[nodiscard]] bool clientbound(protocol::ProtocolState state, MutablePacket& packet) override;

private:
    void writeOlderChunk(const protocol::play::MapChunkPacket& chunk, io::ByteBuffer& output);

    /// Identity on the ids: by the time a link sees a chunk the numbering is already the far end's,
    /// so this only changes how a palette is written.
    ChunkRewriter olderChunks_;
};

} // namespace basalt::wire::multiversion::java
