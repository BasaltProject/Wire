#pragma once

#include <basalt/wire/multiversion/java/RenumberingLink.hpp>

#include <vector>

namespace basalt::wire::multiversion::java {

class Link756 final : public RenumberingLink {
public:
    using RenumberingLink::RenumberingLink;

    [[nodiscard]] bool clientbound(protocol::ProtocolState state, MutablePacket& packet) override;

    [[nodiscard]] bool serverbound(protocol::ProtocolState state, MutablePacket& packet) override;

private:
    /// The simulation distance is 1.18's, and a client of 1.17.1 reads what follows it instead.
    static void dropSimulationDistance(MutablePacket& packet);
};

} // namespace basalt::wire::multiversion::java
