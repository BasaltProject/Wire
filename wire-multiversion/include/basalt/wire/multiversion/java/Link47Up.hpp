#pragma once

#include <basalt/wire/multiversion/java/RenumberingLink.hpp>

#include <array>
#include <cstdint>
#include <string>
#include <unordered_map>

namespace basalt::wire::multiversion::java {

/// A 1.9 client on a 1.8 server: paletted chunks, double positions, reworked entity data and named sounds.
class Link47Up final : public RenumberingLink {
public:
    using RenumberingLink::RenumberingLink;

    [[nodiscard]] bool clientbound(protocol::ProtocolState state, MutablePacket& packet) override;

    [[nodiscard]] bool serverbound(protocol::ProtocolState state, MutablePacket& packet) override;

    [[nodiscard]] bool serverboundForeign(protocol::ProtocolState state, MutablePacket& packet) override;

private:
    struct Boss {
        std::int64_t most;
        std::int64_t least;
        float maxHealth;
    };

    [[nodiscard]] bool chunk(MutablePacket& packet) const;
    [[nodiscard]] bool chunkBulk(MutablePacket& packet) const;
    [[nodiscard]] bool entities(MutablePacket& packet);
    [[nodiscard]] bool spawn(MutablePacket& packet);
    [[nodiscard]] bool entityData(io::ByteBuffer& in, io::ByteBuffer& out, std::int32_t entity, MutablePacket& packet);
    [[nodiscard]] bool vehicle(MutablePacket& packet);
    [[nodiscard]] bool small(MutablePacket& packet);
    void forget(std::int32_t entity, MutablePacket& packet);
    void rememberPlayers(const MutablePacket& packet);

    std::unordered_map<std::int32_t, std::int32_t> kinds_;
    std::unordered_map<std::int32_t, std::int32_t> vehicles_;
    std::unordered_map<std::int32_t, Boss> bosses_;
    /// The players the tab list has named, for entity data that names an owner where 1.9 wants a UUID.
    std::unordered_map<std::string, std::array<std::uint8_t, 16>> players_;
    bool skyLight_ = true;
    std::int32_t teleports_ = 0;
};

} // namespace basalt::wire::multiversion::java
