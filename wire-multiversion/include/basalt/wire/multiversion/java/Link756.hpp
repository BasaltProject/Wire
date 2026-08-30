#pragma once

#include <basalt/wire/multiversion/java/RenumberingLink.hpp>

#include <vector>

namespace basalt::wire::multiversion::java {

/// 1.18 to 1.17.1, where a chunk is a different packet.
///
/// Before 1.18 the biomes are one flat list for the whole column, only the sections with something
/// in them are sent, and a bitmask at the front says which.
///
/// The block entities are left out: 1.17.1 wants each as a tag naming itself and what travels here
/// is a number, so a chest is still a chest but its name does not arrive.
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
