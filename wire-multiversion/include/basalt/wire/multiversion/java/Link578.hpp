#pragma once

#include <basalt/wire/multiversion/java/RenumberingLink.hpp>
#include <basalt/wire/nbt/NbtTag.hpp>

#include <string>
#include <vector>

namespace basalt::wire::multiversion::java {

/// 1.16 to 1.15.2, where a dimension is a number and there are only three of them. Everything 1.16
/// added to describe the world goes away, replaced by the number 1.15.2 knows and by the word it
/// calls the world generator.
///
/// The rest is fields 1.16 added and one packet it merged: equipment leaves as one packet per slot.
class Link578 final : public RenumberingLink {
public:
    using RenumberingLink::RenumberingLink;

    [[nodiscard]] bool clientbound(protocol::ProtocolState state, MutablePacket& packet) override;

    [[nodiscard]] bool serverbound(protocol::ProtocolState state, MutablePacket& packet) override;

private:
    static void rewriteLogin(MutablePacket& packet);
    static void rewriteRespawn(MutablePacket& packet);
    /// 1.16 started saying who said it, which this client does not read.
    static void dropSender(MutablePacket& packet);
    /// The chunk, packed the way everything below 1.16 reads it.
    ///
    /// 1.16 stopped letting a value span two longs, in the sections and in the heightmaps alike.
    /// Every number is the same; only where its bits sit changes. The flag 1.16 added about old
    /// light goes with it.
    static void repackChunk(MutablePacket& packet);
    static void dropTrustEdges(MutablePacket& packet);
    /// One packet per slot, which is what a client this old reads.
    ///
    /// 1.16 put every slot of an entity in one packet, ending the list by leaving the top bit of a
    /// slot clear. The first one stays in this packet and the rest follow it.
    void splitEquipment(MutablePacket& packet) const;
    /// parity: `Attributes` was renamed wholesale in 1.16, and this client knows the old spelling.
    static void renameAttributes(MutablePacket& packet);
};

} // namespace basalt::wire::multiversion::java
