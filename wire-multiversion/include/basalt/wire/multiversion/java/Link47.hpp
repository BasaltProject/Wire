#pragma once

#include <basalt/wire/multiversion/java/RenumberingLink.hpp>

#include <vector>

namespace basalt::wire::multiversion::java {

class Link47 final : public RenumberingLink {
public:
    using RenumberingLink::RenumberingLink;

    [[nodiscard]] bool clientbound(protocol::ProtocolState state, MutablePacket& packet) override;

    [[nodiscard]] bool serverbound(protocol::ProtocolState state, MutablePacket& packet) override;

private:
    [[nodiscard]] bool rewriteChunk(MutablePacket& packet) const;

    static void emptyChunk(MutablePacket& packet);

    [[nodiscard]] static bool rewriteSpawn(MutablePacket& packet);

    static void fixedPointTeleport(MutablePacket& packet);

    [[nodiscard]] static bool rewriteEquipment(MutablePacket& packet);

    static void dropCategory(MutablePacket& packet);
    static void dropTeleportId(MutablePacket& packet);

    void rememberDimension(MutablePacket& packet, bool afterEntityAndGamemode);

    bool hasSkyLight_ = true;
};

} // namespace basalt::wire::multiversion::java
