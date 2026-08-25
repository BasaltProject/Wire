#pragma once

#include <basalt/wire/multiversion/java/RenumberingLink.hpp>

namespace basalt::wire::multiversion::java {

/// A 1.21.2 client on a 1.21 or 1.21.1 server: container ids, spawn fields, positions and the recipe packets.
class Link767Up final : public RenumberingLink {
public:
    using RenumberingLink::RenumberingLink;

    [[nodiscard]] bool clientbound(protocol::ProtocolState state, MutablePacket& packet) override;

    [[nodiscard]] bool serverbound(protocol::ProtocolState state, MutablePacket& packet) override;

private:
    bool dropNextMove_ = false;
};

} // namespace basalt::wire::multiversion::java
