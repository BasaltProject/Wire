#pragma once

#include <basalt/wire/multiversion/ChunkRewriter.hpp>
#include <basalt/wire/multiversion/java/RenumberingLink.hpp>
#include <basalt/wire/nbt/NbtTag.hpp>
#include <basalt/wire/protocol/play/WorldPackets.hpp>

#include <span>

namespace basalt::wire::multiversion::java {

class Link769 final : public RenumberingLink {
public:
    explicit Link769(const VersionTables& tables);

    [[nodiscard]] bool clientbound(protocol::ProtocolState state, MutablePacket& packet) override;

private:
    void writeOlderChunk(const protocol::play::MapChunkPacket& chunk, io::ByteBuffer& output);

    ChunkRewriter olderChunks_;
};

} // namespace basalt::wire::multiversion::java
