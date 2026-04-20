#pragma once

#include <basalt/wire/multiversion/ComposedTables.hpp>
#include <basalt/wire/multiversion/java/RenumberingLink.hpp>
#include <basalt/wire/nbt/NbtTag.hpp>

#include <cstdint>
#include <string>
#include <vector>

namespace basalt::wire::multiversion::java {

class Link736 final : public RenumberingLink {
public:
    Link736(const VersionTables& tables, const ComposedTables& target) : RenumberingLink(tables), target_(&target) {}

    [[nodiscard]] bool clientbound(protocol::ProtocolState state, MutablePacket& packet) override;

private:
    void rewriteLogin(MutablePacket& packet);
    void rewriteRespawn(MutablePacket& packet) const;
    /// parity: MC-119145, where a client this old drops chat aimed above the hotbar on the floor.
    [[nodiscard]] static bool aboveTheHotbar(MutablePacket& packet);
    /// The biomes go out as this client numbers them, and the light flag it still reads comes back.
    void rewriteChunk(MutablePacket& packet) const;
    static void rewriteSectionUpdate(MutablePacket& packet);

    void rememberBiomes(const nbt::NbtCompound& codec);

    [[nodiscard]] std::int32_t biomeAt(std::span<const std::int32_t> ids, std::size_t index) const;

    [[nodiscard]] std::string dimensionName(const nbt::NbtCompound* codec, const nbt::NbtTag& dimension) const;

    /// This server's biome ids to 1.16.1's, filled in from the codec when the player joins.
    std::vector<std::int32_t> biomes_;
    /// The dimensions of the last login, so that a respawn can be named without one going past.
    std::vector<nbt::NbtCompound> known_;
    const ComposedTables* target_;
};

} // namespace basalt::wire::multiversion::java
