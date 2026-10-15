#pragma once

#include <basalt/wire/io/ByteBuffer.hpp>
#include <basalt/wire/multiversion/ProtocolChain.hpp>
#include <basalt/wire/multiversion/VersionTables.hpp>

#include <cstdint>
#include <utility>

namespace basalt::wire::multiversion::java {

/// A link between two versions that differ only in what things are called.
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

    template <typename Read>
    static auto decode(const MutablePacket& packet, Read&& read) {
        io::ByteBuffer input = io::ByteBuffer::wrap(packet.body);
        return std::forward<Read>(read)(input);
    }

    template <typename Write>
    static void encode(MutablePacket& packet, Write&& write) {
        io::ByteBuffer output;
        std::forward<Write>(write)(output);
        packet.body = output.toVector();
    }

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
