#pragma once

#include <basalt/wire/io/ByteBuffer.hpp>
#include <basalt/wire/multiversion/ProtocolChain.hpp>
#include <basalt/wire/multiversion/VersionTables.hpp>

#include <cstdint>
#include <utility>

namespace basalt::wire::multiversion::java {

/// A link between two versions that differ only in what things are called.
///
/// That is most of the modern range: Mojang inserts names into registries and reshuffles packet
/// ids, and the packets themselves stay as they were. The renumbering itself is not done here, it
/// is done once for the whole chain (see ComposedTables); this class exists to carry the id tables
/// and to be the base of the links that do have a packet to reshape.
class RenumberingLink : public ProtocolLink {
public:
    explicit RenumberingLink(const VersionTables& tables)
        : tables_(&tables), removals_{tables.newEntries, tables.newAttributes} {}

    [[nodiscard]] std::int32_t older() const override { return tables_->older; }

    [[nodiscard]] const PacketIdTables& packets() const override { return tables_->packets; }

    [[nodiscard]] const RegistryRemovals& registryRemovals() const override { return removals_; }

    [[nodiscard]] bool clientbound(protocol::ProtocolState, MutablePacket&) override { return true; }

    [[nodiscard]] bool serverbound(protocol::ProtocolState, MutablePacket&) override { return true; }

protected:
    [[nodiscard]] const VersionTables& tables() const noexcept { return *tables_; }

    /// Reads the packet's body without changing it.
    template <typename Read>
    static auto decode(const MutablePacket& packet, Read&& read) {
        io::ByteBuffer input = io::ByteBuffer::wrap(packet.body);
        return std::forward<Read>(read)(input);
    }

    /// Replaces the packet's body with whatever `write` puts in the buffer.
    template <typename Write>
    static void encode(MutablePacket& packet, Write&& write) {
        io::ByteBuffer output;
        std::forward<Write>(write)(output);
        packet.body = output.toVector();
    }

    /// Decodes, lets `block` write the older shape, and puts the result back in the packet.
    template <typename Block>
    static void rewrite(MutablePacket& packet, Block&& block) {
        io::ByteBuffer input = io::ByteBuffer::wrap(packet.body);
        io::ByteBuffer output;
        std::forward<Block>(block)(input, output);
        if (output.readableBytes() > 0) {
            packet.body = output.toVector();
        }
    }

private:
    const VersionTables* tables_;
    RegistryRemovals removals_;
};

} // namespace basalt::wire::multiversion::java
