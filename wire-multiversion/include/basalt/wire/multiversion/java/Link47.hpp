#pragma once

#include <basalt/wire/multiversion/java/RenumberingLink.hpp>

#include <array>
#include <cstdint>
#include <optional>
#include <unordered_map>
#include <vector>

namespace basalt::wire::multiversion::java {

class Link47 final : public RenumberingLink {
public:
    using RenumberingLink::RenumberingLink;

    [[nodiscard]] bool clientbound(protocol::ProtocolState state, MutablePacket& packet) override;

    [[nodiscard]] bool serverbound(protocol::ProtocolState state, MutablePacket& packet) override;

private:
    [[nodiscard]] static bool splitPlacement(MutablePacket& packet);

    std::optional<std::int32_t> teleport_;
    /// The last keep-alives sent, whose 32-bit echoes 1.8 answers with; several can be in flight at once.

    [[nodiscard]] bool rewriteChunk(MutablePacket& packet) const;

    static void emptyChunk(MutablePacket& packet);

    [[nodiscard]] bool rewriteSpawn(MutablePacket& packet);

    void fixedPointTeleport(MutablePacket& packet);

    void remember(std::int32_t entityId, double x, double y, double z, std::uint8_t yaw, std::uint8_t pitch);

    void moveEntity(MutablePacket& packet, bool rotates);

    struct Tracked {
        std::array<std::int64_t, 3> position{};
        std::uint8_t yaw = 0;
        std::uint8_t pitch = 0;
    };

    /// Where each entity is, in 4096ths of a block, so a move too long for 1.8's byte can become a teleport.
    std::unordered_map<std::int32_t, Tracked> positions_;

    [[nodiscard]] static bool rewriteEquipment(MutablePacket& packet);

    static void dropCategory(MutablePacket& packet);
    void dropTeleportId(MutablePacket& packet);

    void rememberDimension(MutablePacket& packet, bool afterEntityAndGamemode);

    bool hasSkyLight_ = true;
};

} // namespace basalt::wire::multiversion::java
