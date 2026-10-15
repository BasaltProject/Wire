#pragma once

#include <basalt/wire/multiversion/ChunkRewriter.hpp>
#include <basalt/wire/multiversion/ComposedTables.hpp>
#include <basalt/wire/multiversion/ProtocolChain.hpp>
#include <basalt/wire/multiversion/SubstitutionLabels.hpp>
#include <basalt/wire/multiversion/java/EntityDataRewriter.hpp>
#include <basalt/wire/protocol/play/InventoryPackets.hpp>

#include <cstdint>
#include <optional>
#include <unordered_map>
#include <vector>

namespace basalt::wire::multiversion::java {

class ContentRewriter final : public PacketContentRewriter {
public:
    ContentRewriter(const ComposedTables& tables, const SubstitutionLabels* labels, std::int32_t farEnd,
                    std::vector<std::int32_t> links);

    [[nodiscard]] bool clientbound(protocol::ProtocolState state, MutablePacket& packet) override;

    /// The way back, for the one packet a client sends that carries an item.
    [[nodiscard]] bool serverbound(protocol::ProtocolState state, MutablePacket& packet) override;

private:
    [[nodiscard]] bool rewriteTags(MutablePacket& packet) const;
    [[nodiscard]] bool rewriteCommands(MutablePacket& packet) const;
    [[nodiscard]] bool rewriteSpawn(MutablePacket& packet);
    [[nodiscard]] bool rewriteRecipes(MutablePacket& packet) const;
    void rewriteAdvancements(MutablePacket& packet) const;
    void rewriteEntityData(MutablePacket& packet) const;

    [[nodiscard]] std::int32_t blockState(std::int32_t id) const;

    [[nodiscard]] std::optional<protocol::play::WireItemStack>
    stack(const std::optional<protocol::play::WireItemStack>& source) const;

    /// Names a substituted item, in this server's numbering so the component travels down the links.
    [[nodiscard]] protocol::play::WireItemStack labelled(protocol::play::WireItemStack item) const;

    /// Names a substituted mob in its own packet after the spawn: one whose state never changes gets no other.
    void nameOver(MutablePacket& packet, std::int32_t entityId, std::int32_t nativeType) const;

    [[nodiscard]] const IdLookup* lookupFor(const std::string& registryId) const;

    const ComposedTables* tables_;
    const SubstitutionLabels* labels_;
    std::int32_t farEnd_;
    EntityDataRewriter entityData_;
    ChunkRewriter chunks_;
    std::unordered_map<std::int32_t, std::int32_t> entityTypes_;
};

} // namespace basalt::wire::multiversion::java
