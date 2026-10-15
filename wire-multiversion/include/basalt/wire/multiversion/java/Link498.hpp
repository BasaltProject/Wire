#pragma once

#include <basalt/wire/multiversion/java/RenumberingLink.hpp>

namespace basalt::wire::multiversion::java {

class Link498 final : public RenumberingLink {
public:
    using RenumberingLink::RenumberingLink;

    [[nodiscard]] bool clientbound(protocol::ProtocolState state, MutablePacket& packet) override;

private:
    /// The biomes go where 1.14 looks for them: after the sections, in the same buffer.
    static void foldBiomesIn(MutablePacket& packet);
    static void dropSeedAndRespawnScreen(MutablePacket& packet);
    static void dropSeed(MutablePacket& packet);
    static void shortenCoordinates(MutablePacket& packet);
    static void appendEmptyData(MutablePacket& packet);
};

} // namespace basalt::wire::multiversion::java
