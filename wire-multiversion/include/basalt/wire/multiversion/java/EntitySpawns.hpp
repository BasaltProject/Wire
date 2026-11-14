#pragma once

#include <basalt/wire/multiversion/ComposedTables.hpp>
#include <basalt/wire/multiversion/ProtocolChain.hpp>
#include <basalt/wire/multiversion/SubstitutionLabels.hpp>
#include <basalt/wire/multiversion/java/EntityDataRewriter.hpp>
#include <basalt/wire/protocol/play/EntityData.hpp>

#include <cstdint>
#include <span>
#include <unordered_map>

namespace basalt::wire::multiversion::java {

/// Which type each entity spawned as, and what the far end is shown in its place.
class EntitySpawns {
public:
    EntitySpawns(const ComposedTables& tables, const SubstitutionLabels* labels, std::int32_t farEnd,
                 const EntityDataRewriter& entityData)
        : tables_(&tables), labels_(labels), farEnd_(farEnd), entityData_(&entityData) {}

    /// Renumbers a spawn for the far end, or says to drop it.
    [[nodiscard]] bool spawn(MutablePacket& packet);

    void player(std::int32_t entityId) { types_[entityId] = protocol::play::SpawnEntityPacket::kPlayerEntityType; }

    void forget(std::span<const std::int32_t> entityIds);

    [[nodiscard]] std::int32_t typeOf(std::int32_t entityId) const;

    static constexpr std::uint8_t kKeepAll = 0xFF;

    /// The last field a substitute shares with the entity it stands for; the rest would land on another's field.
    [[nodiscard]] std::uint8_t lastKept(std::int32_t nativeType) const;

private:
    [[nodiscard]] bool living(std::int32_t nativeType) const noexcept;

    [[nodiscard]] const IdLookup& types(std::int32_t nativeType) const noexcept;

    [[nodiscard]] MutablePacket::Spawn kindOf(std::int32_t nativeType) const noexcept;

    void nameOver(MutablePacket& packet, std::int32_t entityId, std::int32_t nativeType) const;

    const ComposedTables* tables_;
    const SubstitutionLabels* labels_;
    std::int32_t farEnd_;
    const EntityDataRewriter* entityData_;
    std::unordered_map<std::int32_t, std::int32_t> types_;
};

} // namespace basalt::wire::multiversion::java
