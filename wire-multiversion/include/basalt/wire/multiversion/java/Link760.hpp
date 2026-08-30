#pragma once

#include <basalt/wire/multiversion/java/RenumberingLink.hpp>
#include <basalt/wire/protocol/play/PlayPackets.hpp>

namespace basalt::wire::multiversion::java {

/// 1.19.3 to 1.19.1 and 1.19.2, where the tab list is one packet with a mode rather than two.
///
/// 1.19.3 split adding, changing and removing a player into their own packets and let one carry
/// several changes at once. Before that there is a single `player_info` with an action at the front,
/// and each action has its own shape, so what this server sends has to be sorted into them.
class Link760 final : public RenumberingLink {
public:
    using RenumberingLink::RenumberingLink;

    [[nodiscard]] bool clientbound(protocol::ProtocolState state, MutablePacket& packet) override;

private:
    /// Writes the one action a 1.19.1 client can read out of what this server sent.
    ///
    /// Adding a player carries the game mode and the ping with it, so a join is one packet. Anything
    /// else this server sends on its own is a single action too, and anything with no action that
    /// version knows is dropped rather than sent as a mode it would misread.
    [[nodiscard]] static bool writeOneAction(MutablePacket& packet, const protocol::play::PlayerInfoUpdatePacket& info);
};

} // namespace basalt::wire::multiversion::java
