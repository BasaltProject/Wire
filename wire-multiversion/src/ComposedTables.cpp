#include <basalt/wire/multiversion/ComposedTables.hpp>

namespace basalt::wire::multiversion {
namespace {

constexpr std::string_view kWholeRegistry = "/*";
constexpr std::int32_t kFluidCountSince = 775;

/// Collects one registry's map out of every link, in chain order.
template <typename Member>
ComposedIdMap composeOne(std::span<const std::reference_wrapper<const VersionTables>> tables, Member member) {
    std::vector<std::reference_wrapper<const IdMap>> steps;
    steps.reserve(tables.size());
    for (const VersionTables& table : tables) {
        steps.emplace_back(table.*member);
    }
    return ComposedIdMap{std::move(steps)};
}

} // namespace

std::int32_t ComposedIdMap::map(std::int32_t id) const {
    std::int32_t current = id;
    for (const IdMap& step : steps_) {
        current = step.map(current);
        if (current == kNone) {
            return kNone;
        }
    }
    return current;
}

bool ComposedIdMap::substitutes(std::int32_t id) const {
    std::int32_t current = id;
    for (const IdMap& step : steps_) {
        if (step.substitutes(current)) {
            return true;
        }
        current = step.map(current);
        if (current == kNone) {
            return false;
        }
    }
    return false;
}

std::int32_t ComposedIdMap::unmap(std::int32_t id) const {
    std::int32_t current = id;
    for (auto step = steps_.rbegin(); step != steps_.rend(); ++step) {
        current = step->get().unmap(current);
        if (current == kNone) {
            return kNone;
        }
    }
    return current;
}

ComposedTables::ComposedTables(std::span<const std::reference_wrapper<const VersionTables>> tables)
    : blockStates(composeOne(tables, &VersionTables::blockStates)), block(composeOne(tables, &VersionTables::block)),
      item(composeOne(tables, &VersionTables::item)), particleType(composeOne(tables, &VersionTables::particleType)),
      entityType(composeOne(tables, &VersionTables::entityType)),
      attribute(composeOne(tables, &VersionTables::attribute)),
      dataComponentType(composeOne(tables, &VersionTables::dataComponentType)),
      menu(composeOne(tables, &VersionTables::menu)), mobEffect(composeOne(tables, &VersionTables::mobEffect)),
      soundEvent(composeOne(tables, &VersionTables::soundEvent)),
      gameEvent(composeOne(tables, &VersionTables::gameEvent)),
      slotDisplay(composeOne(tables, &VersionTables::slotDisplay)) {
    if (tables.empty()) {
        return;
    }

    const VersionTables& last = tables.back();
    const VersionTables& first = tables.front();

    attributeNames_ = &last.attributeNames;
    staticBiomes_ = &last.staticBiomes;
    playerEntityType_ = last.playerEntityType;
    nativePlayerEntityType_ = first.newerPlayerEntityType;
    blockStateDirectBits_ = last.blockStateDirectBits;
    fluidCountInSections_ = last.older >= kFluidCountSince;

    for (const VersionTables& table : tables) {
        for (const std::string& entry : table.newEntries.names()) {
            if (entry.ends_with(kWholeRegistry)) {
                missingRegistries_.insert(entry.substr(0, entry.size() - kWholeRegistry.size()));
            }
        }
    }
}

bool ComposedTables::isPlayer(std::int32_t id) const noexcept {
    return playerEntityType_ >= 0 ? id == playerEntityType_ : id == nativePlayerEntityType_;
}

} // namespace basalt::wire::multiversion
