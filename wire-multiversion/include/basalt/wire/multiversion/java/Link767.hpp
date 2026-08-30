#pragma once

#include <basalt/wire/multiversion/java/RenumberingLink.hpp>
#include <basalt/wire/protocol/play/SpawnInfo.hpp>

namespace basalt::wire::multiversion::java {

/// 1.21.2 to 1.21, the deepest step of the modern range.
///
/// 1.21.2 rewrote how a position reaches a client, gave the world a sea level and the login a
/// strictness flag, and made the container id a varint where 1.21 reads one byte.
///
/// Dropped rather than sent wrong: the recipe book, explosions and item cooldowns.
class Link767 final : public RenumberingLink {
public:
    using RenumberingLink::RenumberingLink;

    [[nodiscard]] bool clientbound(protocol::ProtocolState state, MutablePacket& packet) override;

    [[nodiscard]] bool serverbound(protocol::ProtocolState state, MutablePacket& packet) override;

private:
    /// The container a packet is about, as the one byte 1.21.1 reads.
    ///
    /// 1.21.2 made it a varint, and the two agree on every container there is except the one that
    /// matters: minus one, the player's own screen, is one byte there and five here.
    static void narrowContainer(MutablePacket& packet);

    /// The same the other way, for what the client sends back about a container.
    static void widenContainer(MutablePacket& packet);

    /// Everything a spawn tells the client, minus the sea level 1.21.2 added at the end.
    static void writeSpawnInfoWithoutSeaLevel(io::ByteBuffer& output, const protocol::play::SpawnInfo& spawn);
};

} // namespace basalt::wire::multiversion::java
