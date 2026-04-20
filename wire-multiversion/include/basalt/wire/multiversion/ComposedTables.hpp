#pragma once

#include <basalt/wire/multiversion/VersionTables.hpp>

#include <cstdint>
#include <functional>
#include <set>
#include <span>
#include <string>
#include <vector>

namespace basalt::wire::multiversion {

/// Several links' numbering, applied one after another as if it were a single table.
class ComposedIdMap final : public IdLookup {
public:
    ComposedIdMap() = default;

    explicit ComposedIdMap(std::vector<std::reference_wrapper<const IdMap>> steps) : steps_(std::move(steps)) {}

    [[nodiscard]] std::int32_t map(std::int32_t id) const override;

    [[nodiscard]] bool substitutes(std::int32_t id) const override;

    /// The same walk backwards, for an id arriving from the client.
    [[nodiscard]] std::int32_t unmap(std::int32_t id) const override;

private:
    std::vector<std::reference_wrapper<const IdMap>> steps_;
};

class ComposedTables {
public:
    explicit ComposedTables(std::span<const std::reference_wrapper<const VersionTables>> tables);

    ComposedIdMap blockStates;
    ComposedIdMap block;
    ComposedIdMap item;
    ComposedIdMap particleType;
    ComposedIdMap entityType;
    ComposedIdMap attribute;
    ComposedIdMap dataComponentType;
    ComposedIdMap menu;
    ComposedIdMap mobEffect;
    ComposedIdMap soundEvent;
    ComposedIdMap gameEvent;
    ComposedIdMap slotDisplay;

    [[nodiscard]] const NameIdMap& attributeNames() const noexcept { return *attributeNames_; }

    [[nodiscard]] const NameIdMap& staticBiomes() const noexcept { return *staticBiomes_; }

    [[nodiscard]] const std::set<std::string>& missingRegistries() const noexcept { return missingRegistries_; }

    /// Whether an entity type is a player, which is the only one a link has to recognise.
    [[nodiscard]] bool isPlayer(std::int32_t id) const noexcept;

    [[nodiscard]] std::int32_t nativePlayerEntityType() const noexcept { return nativePlayerEntityType_; }

    [[nodiscard]] int blockStateDirectBits() const noexcept { return blockStateDirectBits_; }

    /// Anything below it reads one short where this server writes two.
    [[nodiscard]] bool fluidCountInSections() const noexcept { return fluidCountInSections_; }

private:
    const NameIdMap* attributeNames_ = nullptr;
    const NameIdMap* staticBiomes_ = nullptr;
    std::set<std::string> missingRegistries_;
    std::int32_t playerEntityType_ = -1;
    std::int32_t nativePlayerEntityType_ = -1;
    int blockStateDirectBits_ = 15;
    bool fluidCountInSections_ = false;
};

} // namespace basalt::wire::multiversion
