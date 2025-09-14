#pragma once

#include <basalt/wire/multiversion/java/RenumberingLink.hpp>

#include <vector>

namespace basalt::wire::multiversion::java {

/// 1.9 to 1.8, where a position is fixed point and a chunk has no palette.
///
/// 1.8 writes every block as a short of `id << 4 | metadata`, and the light of all the sections
/// behind all of them rather than section by section, so the chunk is decoded and written again.
///
/// The data an entity carries does not travel: 1.8 packs it a different way entirely.
class Link47 final : public RenumberingLink {
public:
    using RenumberingLink::RenumberingLink;

    [[nodiscard]] bool clientbound(protocol::ProtocolState state, MutablePacket& packet) override;

    [[nodiscard]] bool serverbound(protocol::ProtocolState state, MutablePacket& packet) override;

private:
    /// The chunk as 1.8 reads it: every block a short, and the light behind all of them.
    ///
    /// The values arrive packed the old way (a value may span two longs), which is what everything
    /// below 1.16 uses, so they are unpacked, looked up in the palette and written out flat.
    [[nodiscard]] bool rewriteChunk(MutablePacket& packet) const;

    /// parity: 1.8 forgets a chunk by being sent an empty one, because it has no packet for it.
    static void emptyChunk(MutablePacket& packet);

    /// A spawn, with its position in thirty-seconds of a block and no uuid.
    ///
    /// Which of the three spawns this is has to be read off the number the versions above gave it:
    /// this server has one packet for all of them and the links above split it.
    [[nodiscard]] static bool rewriteSpawn(MutablePacket& packet);

    /// A teleport, with its position in thirty-seconds of a block.
    ///
    /// Both of this server's ways of saying where something is are one packet by the time they get
    /// here: the link to 1.21 turned the exact one into a teleport, and the angles are already the
    /// byte each that every version below it reads.
    static void fixedPointTeleport(MutablePacket& packet);

    /// 1.8 has five slots and no second hand, and numbers them from the one being held.
    [[nodiscard]] static bool rewriteEquipment(MutablePacket& packet);

    static void dropCategory(MutablePacket& packet);
    static void dropTeleportId(MutablePacket& packet);

    /// Whether this world has a sky, which is what says if a chunk carries sky light.
    void rememberDimension(MutablePacket& packet, bool afterEntityAndGamemode);

    bool hasSkyLight_ = true;
};

} // namespace basalt::wire::multiversion::java
