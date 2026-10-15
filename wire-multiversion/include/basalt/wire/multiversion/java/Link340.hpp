#pragma once

#include <basalt/wire/multiversion/java/RenumberingLink.hpp>

#include <vector>

namespace basalt::wire::multiversion::java {

/// Dropped rather than sent wrong: particles, statistics and maps, whose payloads carry no meaning across.
class Link340 final : public RenumberingLink {
public:
    using RenumberingLink::RenumberingLink;

    [[nodiscard]] bool clientbound(protocol::ProtocolState state, MutablePacket& packet) override;

    [[nodiscard]] bool serverbound(protocol::ProtocolState state, MutablePacket& packet) override;

private:
    static void narrowBiomes(MutablePacket& packet);
};

} // namespace basalt::wire::multiversion::java
