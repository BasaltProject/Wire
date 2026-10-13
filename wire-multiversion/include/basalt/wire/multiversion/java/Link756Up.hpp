#pragma once

#include <basalt/wire/multiversion/java/RenumberingLink.hpp>

#include <cstdint>
#include <map>
#include <set>
#include <utility>
#include <vector>

namespace basalt::wire::nbt {
class NbtTag;
}

namespace basalt::wire::multiversion::java {

/// A 1.18 client on a 1.17.1 server: biomes and light move into the chunk packet and the world can be any height.
class Link756Up final : public RenumberingLink {
public:
    using RenumberingLink::RenumberingLink;

    [[nodiscard]] bool clientbound(protocol::ProtocolState state, MutablePacket& packet) override;

    [[nodiscard]] bool serverbound(protocol::ProtocolState state, MutablePacket& packet) override;

private:
    using ChunkKey = std::pair<std::int32_t, std::int32_t>;

    void readWorld(const nbt::NbtTag& dimension);
    void login(MutablePacket& packet);
    void respawn(MutablePacket& packet);
    [[nodiscard]] bool light(MutablePacket& packet);
    [[nodiscard]] bool chunk(MutablePacket& packet);
    void blockEntity(MutablePacket& packet);

    std::int32_t sections_ = 16;
    int biomeBits_ = 6;
    std::set<ChunkKey> loaded_;
    std::map<ChunkKey, std::vector<std::uint8_t>> pendingLight_;
};

} // namespace basalt::wire::multiversion::java
