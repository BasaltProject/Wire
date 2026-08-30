#pragma once

#include <basalt/wire/multiversion/java/RenumberingLink.hpp>

namespace basalt::wire::multiversion::java {

/// 1.19.1 to 1.19. One field, and it is the one that says where a message goes.
///
/// 1.19.1 turned the kind of a system message into a flag: on the action bar, or not. Before it the
/// kind is a number out of the chat type registry, so the flag becomes the number a client of 1.19
/// looks up.
class Link759 final : public RenumberingLink {
public:
    using RenumberingLink::RenumberingLink;

    [[nodiscard]] bool clientbound(protocol::ProtocolState state, MutablePacket& packet) override;
};

} // namespace basalt::wire::multiversion::java
