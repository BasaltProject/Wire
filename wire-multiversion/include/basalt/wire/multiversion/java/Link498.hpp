#pragma once

#include <basalt/wire/multiversion/java/RenumberingLink.hpp>

namespace basalt::wire::multiversion::java {

/// 1.15 to 1.14.4, where a biome is still a column and an entity brings its own data.
///
/// 1.14 has one biome per column and reads them after the sections, so 1.15's thousand and twenty
/// four become two hundred and fifty six, taken from the bottom of each.
///
/// A spawn gets its empty set of entity data back, and the world seed goes.
class Link498 final : public RenumberingLink {
public:
    using RenumberingLink::RenumberingLink;

    [[nodiscard]] bool clientbound(protocol::ProtocolState state, MutablePacket& packet) override;

private:
    /// The biomes go where 1.14 looks for them: after the sections, in the same buffer.
    static void foldBiomesIn(MutablePacket& packet);
    static void dropSeedAndRespawnScreen(MutablePacket& packet);
    static void dropSeed(MutablePacket& packet);
    /// 1.15 made a particle's position a double; 1.14 reads three floats.
    static void shortenCoordinates(MutablePacket& packet);
    /// An empty set of entity data, which 1.14 expects at the end of a spawn.
    ///
    /// What the entity actually looks like arrives right after in its own packet, so nothing is lost
    /// by starting it empty: the client reads the terminator and waits for that one.
    static void appendEmptyData(MutablePacket& packet);
};

} // namespace basalt::wire::multiversion::java
