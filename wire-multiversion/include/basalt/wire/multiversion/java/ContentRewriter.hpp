#pragma once

#include <basalt/wire/multiversion/ChunkRewriter.hpp>
#include <basalt/wire/multiversion/ComposedTables.hpp>
#include <basalt/wire/multiversion/ProtocolChain.hpp>
#include <basalt/wire/multiversion/ProtocolVersions.hpp>
#include <basalt/wire/multiversion/SubstitutionLabels.hpp>
#include <basalt/wire/multiversion/java/BlockEntityStates.hpp>
#include <basalt/wire/multiversion/java/EntityDataRewriter.hpp>
#include <basalt/wire/multiversion/java/EntitySpawns.hpp>
#include <basalt/wire/multiversion/java/OlderPaintings.hpp>
#include <basalt/wire/protocol/play/InventoryPackets.hpp>

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace basalt::wire::multiversion::java {

class ContentRewriter final : public PacketContentRewriter {
public:
    ContentRewriter(const ComposedTables& tables, const SubstitutionLabels* labels, std::int32_t farEnd,
                    std::vector<std::int32_t> links, bool backendCountsFluids = true);

    [[nodiscard]] bool clientbound(protocol::ProtocolState state, MutablePacket& packet) override;

    /// The way back, for the one packet a client sends that carries an item.
    [[nodiscard]] bool serverbound(protocol::ProtocolState state, MutablePacket& packet) override;

private:
    [[nodiscard]] bool rewriteClientbound(protocol::ProtocolState state, MutablePacket& packet);

    /// From 1.17.1 down a block entity is named in its tag, and from 1.21.9 up it is numbered as here.
    [[nodiscard]] std::uint8_t newestGameEvent() const noexcept;

    [[nodiscard]] bool renumbersBlockEntities() const noexcept {
        return farEnd_ >= protocolVersions::kV1_18 && farEnd_ < protocolVersions::kV1_21_9;
    }

    [[nodiscard]] bool rewriteTags(MutablePacket& packet) const;
    [[nodiscard]] bool rewriteCommands(MutablePacket& packet) const;
    [[nodiscard]] bool rewriteRecipes(MutablePacket& packet) const;
    void rewriteAdvancements(MutablePacket& packet) const;
    void rewriteEntityData(MutablePacket& packet) const;

    [[nodiscard]] std::int32_t blockState(std::int32_t id) const;

    /// Before 26.1 a particle is written whole, in the shape its era reads, and left alone by the links.
    [[nodiscard]] bool olderParticle(MutablePacket& packet, std::int32_t nativeType, std::int32_t mapped) const;

    [[nodiscard]] std::optional<protocol::play::WireItemStack>
    stack(const std::optional<protocol::play::WireItemStack>& source) const;

    /// Names a substituted item, in this server's numbering so the component travels down the links.
    [[nodiscard]] protocol::play::WireItemStack labelled(protocol::play::WireItemStack item) const;

    [[nodiscard]] const IdLookup* lookupFor(const std::string& registryId) const;

    const ComposedTables* tables_;
    const SubstitutionLabels* labels_;
    std::int32_t farEnd_;
    EntityDataRewriter entityData_;
    EntitySpawns spawns_;
    OlderPaintings paintings_;
    bool countsFluids_;
    ChunkRewriter chunks_;
    BlockEntityStates placed_;
    std::vector<std::string> enchantments_;
};

} // namespace basalt::wire::multiversion::java
