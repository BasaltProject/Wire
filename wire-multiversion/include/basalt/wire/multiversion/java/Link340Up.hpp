#pragma once

#include <basalt/wire/multiversion/java/Connections340.hpp>
#include <basalt/wire/multiversion/java/RenumberingLink.hpp>

#include <cstdint>
#include <string>
#include <unordered_map>

namespace basalt::wire::multiversion::java {

/// A 1.13 client on a 1.12.2 server: blocks and items flattened, and what 1.12 kept in block entities put in states.
class Link340Up final : public RenumberingLink {
public:
    using RenumberingLink::RenumberingLink;

    [[nodiscard]] bool clientbound(protocol::ProtocolState state, MutablePacket& packet) override;

    [[nodiscard]] bool serverbound(protocol::ProtocolState state, MutablePacket& packet) override;

    [[nodiscard]] bool serverboundForeign(protocol::ProtocolState state, MutablePacket& packet) override;

private:
    [[nodiscard]] bool world(MutablePacket& packet);
    [[nodiscard]] bool chunk(MutablePacket& packet);
    [[nodiscard]] bool blockChange(MutablePacket& packet);
    [[nodiscard]] bool blockChanges(MutablePacket& packet);
    [[nodiscard]] bool blockEntity(MutablePacket& packet);
    [[nodiscard]] bool particles(MutablePacket& packet) const;
    [[nodiscard]] bool entities(MutablePacket& packet);
    [[nodiscard]] bool entityData(io::ByteBuffer& in, io::ByteBuffer& out, std::int32_t kind) const;
    [[nodiscard]] bool items(MutablePacket& packet) const;
    [[nodiscard]] bool customPayload(MutablePacket& packet) const;
    [[nodiscard]] bool scoreboard(MutablePacket& packet) const;
    [[nodiscard]] bool suggestions(MutablePacket& packet) const;
    void joined(MutablePacket& packet);

    /// The 1.13 state for a 1.12 one, given what the block entity at that place says.
    [[nodiscard]] std::int32_t stateAt(std::int64_t position, std::int32_t legacy) const;
    /// The same, joined to its neighbours as 1.13 would show it.
    [[nodiscard]] std::int32_t shownAt(std::int32_t x, std::int32_t y, std::int32_t z, std::int32_t legacy) const;
    void remember(std::int64_t position, std::int32_t legacy);
    void forgetChunk(std::int32_t x, std::int32_t z);
    /// Sends again the blocks next to a change whose look depends on it.
    void refreshAround(MutablePacket& packet, std::int32_t x, std::int32_t y, std::int32_t z) const;
    /// The same for the edges of the chunks next to one that just arrived.
    void refreshEdges(MutablePacket& packet, std::int32_t chunkX, std::int32_t chunkZ) const;

    std::unordered_map<std::int32_t, std::int32_t> kinds_;
    connections340::LegacyWorld world_;
    /// Blocks whose 1.13 state needs their block entity, by position, and that entity's data.
    std::unordered_map<std::int64_t, std::int32_t> legacyAt_;
    std::unordered_map<std::int64_t, std::int32_t> variantAt_;
    bool skyLight_ = true;
    std::int32_t suggestionId_ = 0;
    std::string suggestionInput_;
};

} // namespace basalt::wire::multiversion::java
