#pragma once

#include <basalt/wire/multiversion/java/RenumberingLink.hpp>
#include <basalt/wire/protocol/play/SpawnInfo.hpp>

namespace basalt::wire::multiversion::java {

/// Dropped rather than sent wrong: the recipe book, explosions and item cooldowns.
class Link767 final : public RenumberingLink {
public:
    using RenumberingLink::RenumberingLink;

    [[nodiscard]] bool clientbound(protocol::ProtocolState state, MutablePacket& packet) override;

    [[nodiscard]] bool serverbound(protocol::ProtocolState state, MutablePacket& packet) override;

private:
    static void narrowContainer(MutablePacket& packet);

    /// The same the other way, for what the client sends back about a container.
    static void widenContainer(MutablePacket& packet);
};

} // namespace basalt::wire::multiversion::java
