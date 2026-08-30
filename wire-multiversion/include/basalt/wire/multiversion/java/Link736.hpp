#pragma once

#include <basalt/wire/multiversion/ComposedTables.hpp>
#include <basalt/wire/multiversion/java/RenumberingLink.hpp>
#include <basalt/wire/nbt/NbtTag.hpp>

#include <string>
#include <vector>

namespace basalt::wire::multiversion::java {

/// 1.16.2 to 1.16.1, where the world stops being described to the client.
///
/// The dimension goes out as a name, the biomes are translated through theirs against the list the
/// client has compiled in, hardcore goes back to being a bit of the gamemode, and a line meant for
/// above the hotbar is sent as a title, which is the only way this client shows one (MC-119145).
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
    /// 1.16.2 packed a whole section into one long; 1.16.1 names the chunk and each block in it.
    static void rewriteSectionUpdate(MutablePacket& packet);

    void rememberBiomes(const nbt::NbtCompound& codec);

    [[nodiscard]] std::int32_t biomeAt(std::span<const std::int32_t> ids, std::size_t index) const;

    /// Which dimension the settings that just went past belong to.
    ///
    /// Matched against the registry rather than guessed from its contents: the settings sent in the
    /// login packet are one of the entries, so the entry that equals them is the answer.
    [[nodiscard]] std::string dimensionName(const nbt::NbtCompound* codec, const nbt::NbtTag& dimension) const;

    /// This server's biome ids to 1.16.1's, filled in from the codec when the player joins.
    std::vector<std::int32_t> biomes_;
    /// The dimensions of the last login, so that a respawn can be named without one going past.
    std::vector<nbt::NbtCompound> known_;
    const ComposedTables* target_;
};

} // namespace basalt::wire::multiversion::java
