#pragma once

#include <basalt/wire/multiversion/java/RenumberingLink.hpp>
#include <basalt/wire/protocol/play/SpawnInfo.hpp>

namespace basalt::wire::multiversion::java {

/// 1.21.2 to 1.21, the deepest step of the modern range. 1.21.2 rewrote position sync, added sea
/// level and a login strictness flag, and made the container id a varint (1.21 reads a byte).
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
};

} // namespace basalt::wire::multiversion::java
