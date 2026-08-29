#pragma once

#include <basalt/wire/multiversion/java/RenumberingLink.hpp>

namespace basalt::wire::multiversion::java {

/// A 1.20.2 client on a 1.20 or 1.20.1 server: network NBT without a root name, players as entities, login order.
class Link763Up final : public RenumberingLink {
public:
    using RenumberingLink::RenumberingLink;

    [[nodiscard]] bool clientbound(protocol::ProtocolState state, MutablePacket& packet) override;

    [[nodiscard]] bool serverbound(protocol::ProtocolState state, MutablePacket& packet) override;
};

} // namespace basalt::wire::multiversion::java
