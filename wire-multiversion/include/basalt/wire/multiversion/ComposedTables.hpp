#pragma once

#include <basalt/wire/multiversion/VersionTables.hpp>

#include <functional>
#include <set>
#include <span>
#include <string>
#include <vector>

namespace basalt::wire::multiversion {

/// Several links' numbering, applied one after another as if it were a single table.
///
/// A chain of five links moves an id five times, and every step is a lookup in that step's own
/// table. Composing them is what lets the renumbering happen **once**, on the packet as this server
/// wrote it, instead of once per link: a link halfway down would otherwise be reading a packet the
/// link above it had already rewritten, which is how a chunk ends up parsed as something it is not.
class ComposedIdMap final : public IdLookup {
public:
    ComposedIdMap() = default;

    explicit ComposedIdMap(std::vector<std::reference_wrapper<const IdMap>> steps) : steps_(std::move(steps)) {}

    [[nodiscard]] std::int32_t map(std::int32_t id) const override;

    /// True as soon as one step had to stand something else in, which the whole walk inherits.
    [[nodiscard]] bool substitutes(std::int32_t id) const override;

    /// The same walk backwards, for an id arriving from the client.
    [[nodiscard]] std::int32_t unmap(std::int32_t id) const override;

private:
    std::vector<std::reference_wrapper<const IdMap>> steps_;
};

/// Every registry a packet can carry an id of, composed across a whole chain.
///
/// Holds references into the tables it was built from, which are static for the life of the process:
/// a chain is built once per connection and the tables it walks are not.
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

    /// The far end's own names for what it reads by name rather than by number. The far end's and
    /// not each link's: the renumbering happens once, so by the time a link sees an id it is already
    /// the number the client will read, and only that version's names fit it.
    [[nodiscard]] const NameIdMap& attributeNames() const noexcept { return *attributeNames_; }

    [[nodiscard]] const NameIdMap& staticBiomes() const noexcept { return *staticBiomes_; }

    /// Registries the far end does not have at all, by name.
    ///
    /// A command whose argument names one of them is a command the client cannot read: it looks the
    /// registry up while parsing the tree and gives up on the whole packet when it is not there,
    /// which costs the connection.
    [[nodiscard]] const std::set<std::string>& missingRegistries() const noexcept { return missingRegistries_; }

    /// Whether an entity type is a player, which is the only one a link has to recognise.
    ///
    /// Below 1.13 a player has no number of its own: it is not in the registry there because it is
    /// never spawned as an entity, it has a packet of its own. So the number that arrives is this
    /// build's, passed through untouched, and that is what is compared.
    [[nodiscard]] bool isPlayer(std::int32_t id) const noexcept;

    /// What *this* build numbers a player, which is the top of the chain whatever it translates to.
    [[nodiscard]] std::int32_t nativePlayerEntityType() const noexcept { return nativePlayerEntityType_; }

    /// How wide a direct block palette is at the far end of the chain.
    [[nodiscard]] int blockStateDirectBits() const noexcept { return blockStateDirectBits_; }

    /// Whether the far end reads the fluid count 26.1 added to every chunk section. Anything below
    /// it reads one short where this server writes two.
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
