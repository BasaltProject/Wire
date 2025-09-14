#pragma once

#include <basalt/wire/multiversion/java/RenumberingLink.hpp>

namespace basalt::wire::multiversion::java {

/// 26.2 to 26.1. Nothing in play changed, but the login did.
///
/// 26.2 put a session id at the end of the packet that finishes logging in, and a flag saying
/// whether the server is in online mode at the end of the one that starts the game. Every version
/// below reads neither, so both have to go: a client left with a packet longer than the one it
/// knows stops at the last thing it reads before the game starts.
class Link775 final : public RenumberingLink {
public:
    using RenumberingLink::RenumberingLink;

    [[nodiscard]] bool clientbound(protocol::ProtocolState state, MutablePacket& packet) override;
};

} // namespace basalt::wire::multiversion::java
