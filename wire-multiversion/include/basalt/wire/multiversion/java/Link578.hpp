#pragma once

#include <basalt/wire/multiversion/java/RenumberingLink.hpp>
#include <basalt/wire/nbt/NbtTag.hpp>

#include <string>
#include <vector>

namespace basalt::wire::multiversion::java {

class Link578 final : public RenumberingLink {
public:
    using RenumberingLink::RenumberingLink;

    [[nodiscard]] bool clientbound(protocol::ProtocolState state, MutablePacket& packet) override;

    [[nodiscard]] bool serverbound(protocol::ProtocolState state, MutablePacket& packet) override;

private:
    static void rewriteLogin(MutablePacket& packet);
    static void rewriteRespawn(MutablePacket& packet);
    static void dropSender(MutablePacket& packet);
    /// Every number is the same; only where its bits sit changes.
    static void repackChunk(MutablePacket& packet);
    static void dropTrustEdges(MutablePacket& packet);
    /// One packet per slot, which is what a client this old reads.
    void splitEquipment(MutablePacket& packet) const;
    /// parity: `Attributes` was renamed wholesale in 1.16, and this client knows the old spelling.
    static void renameAttributes(MutablePacket& packet);
};

} // namespace basalt::wire::multiversion::java
