#pragma once

#include <basalt/wire/multiversion/java/RenumberingLink.hpp>

#include <cstdint>

namespace basalt::wire::multiversion::java {

/// A 26.1 client on a 1.21.11 server: clocks, split interactions and a few registries that grew.
class Link774Up final : public RenumberingLink {
public:
    using RenumberingLink::RenumberingLink;

    [[nodiscard]] bool clientbound(protocol::ProtocolState state, MutablePacket& packet) override;

    [[nodiscard]] bool serverbound(protocol::ProtocolState state, MutablePacket& packet) override;

    [[nodiscard]] bool serverboundForeign(protocol::ProtocolState state, MutablePacket& packet) override;

private:
    [[nodiscard]] bool configuration(MutablePacket& packet) const;

    bool sneaking_ = false;
};

} // namespace basalt::wire::multiversion::java
