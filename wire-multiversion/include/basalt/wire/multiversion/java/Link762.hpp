#pragma once

#include <basalt/wire/multiversion/java/RenumberingLink.hpp>

namespace basalt::wire::multiversion::java {

/// 1.20 to 1.19.4. Small fields, in and out.
///
/// 1.20 took the "trust edges" flag out of the light that comes with a chunk, and added a cooldown
/// to what the portal remembers when a player spawns. A sign gained a side, and the packet that says
/// who killed you lost the killer.
class Link762 final : public RenumberingLink {
public:
    using RenumberingLink::RenumberingLink;

    [[nodiscard]] bool clientbound(protocol::ProtocolState state, MutablePacket& packet) override;

    [[nodiscard]] bool serverbound(protocol::ProtocolState state, MutablePacket& packet) override;

private:
    /// Rewrites the packet the link below built, minus the portal cooldown 1.20 added at its end.
    ///
    /// Both of them end with it, and both carry a spawn that is already named rather than numbered,
    /// so this walks the shape rather than the packet class: by here it is no longer one.
    static void dropPortalCooldown(MutablePacket& packet, bool afterLogin);
};

} // namespace basalt::wire::multiversion::java
