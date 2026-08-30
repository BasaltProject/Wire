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

/// Puts every id a packet carries into the far end's numbering, once on the packet as this server
/// wrote it rather than once per link: a link in the middle cannot read fields a link above it has
/// already rewritten, so the numbering is composed and only shape changes are touched per link.
///
/// One of these belongs to one connection, because it remembers what the entities in sight are.
class ContentRewriter final : public PacketContentRewriter {
public:
    /// `labels` may be null, which leaves substitutes unnamed, and is what an operator who turned
    /// the naming off wants. `farEnd` is the protocol at the far end, and `links` the older side of
    /// every link between here and there, newest first.
    ContentRewriter(const ComposedTables& tables, const SubstitutionLabels* labels, std::int32_t farEnd,
                    std::vector<std::int32_t> links);

    /// False drops the packet: the far end has no id for what this one is about.
    [[nodiscard]] bool clientbound(protocol::ProtocolState state, MutablePacket& packet) override;

    /// The way back, for the one packet a client sends that carries an item.
    [[nodiscard]] bool serverbound(protocol::ProtocolState state, MutablePacket& packet) override;

private:
    [[nodiscard]] bool rewriteTags(MutablePacket& packet) const;
    [[nodiscard]] bool rewriteCommands(MutablePacket& packet) const;
    [[nodiscard]] bool rewriteSpawn(MutablePacket& packet);
    [[nodiscard]] bool rewriteRecipes(MutablePacket& packet) const;
    void rewriteEntityData(MutablePacket& packet) const;

    [[nodiscard]] std::int32_t blockState(std::int32_t id) const;

    [[nodiscard]] std::optional<protocol::play::WireItemStack>
    stack(const std::optional<protocol::play::WireItemStack>& source) const;

    /// Names a substituted item, in this server's numbering so the component travels down the links.
    [[nodiscard]] protocol::play::WireItemStack labelled(protocol::play::WireItemStack item) const;

    /// Names a substituted mob in its own packet after the spawn: one whose state never changes gets
    /// no other.
    void nameOver(MutablePacket& packet, std::int32_t entityId, std::int32_t nativeType) const;

    [[nodiscard]] const IdLookup* lookupFor(const std::string& registryId) const;

    const ComposedTables* tables_;
    const SubstitutionLabels* labels_;
    EntityDataRewriter entityData_;
    ChunkRewriter chunks_;
    /// What each entity in sight really is, which says where its data sits over there.
    std::unordered_map<std::int32_t, std::int32_t> entityTypes_;
};

} // namespace basalt::wire::multiversion::java
