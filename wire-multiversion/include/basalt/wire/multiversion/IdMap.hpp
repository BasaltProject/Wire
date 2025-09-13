#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace basalt::wire::multiversion {

/// One numbering seen from another, whether it is a single step or a whole chain of them.
class IdLookup {
public:
    /// No id at all on the other side, and no substitute either.
    static constexpr std::int32_t kNone = -1;

    IdLookup() = default;
    IdLookup(const IdLookup&) = default;
    IdLookup(IdLookup&&) = default;
    IdLookup& operator=(const IdLookup&) = default;
    IdLookup& operator=(IdLookup&&) = default;
    virtual ~IdLookup() = default;

    /// The id the other side uses, or none when it has nothing to show.
    [[nodiscard]] virtual std::int32_t map(std::int32_t id) const = 0;

    [[nodiscard]] virtual std::int32_t unmap(std::int32_t id) const = 0;

    /// Whether the map answered with something else standing in, rather than with this same thing.
    [[nodiscard]] virtual bool substitutes(std::int32_t id) const = 0;
};

/// What one version's numbering for a registry becomes in the version below. Ids barely move one at
/// a time, so this is stored as runs of constant difference plus the leftovers, which turns thirty
/// thousand block states into fifty-five runs.
///
/// Built from `start:length:delta` runs, then a pipe, then `native>old` for whatever is shown as
/// something else. Parsed on first use, so a link nobody needs costs nothing.
class IdMap final : public IdLookup {
public:
    explicit IdMap(std::string encoded) : encoded_(std::move(encoded)) {}

    [[nodiscard]] std::int32_t map(std::int32_t id) const override;

    [[nodiscard]] std::int32_t unmap(std::int32_t id) const override;

    [[nodiscard]] bool substitutes(std::int32_t id) const override;

    /// A map that changes nothing, for a registry a release left alone.
    [[nodiscard]] static const IdMap& identity();

private:
    void parse() const;

    std::string encoded_;
    // Parsed lazily, so the members are mutable: the object is logically const throughout.
    mutable std::vector<std::int32_t> starts_;
    mutable std::vector<std::int32_t> lengths_;
    mutable std::vector<std::int32_t> deltas_;
    mutable std::vector<std::int32_t> substituteFrom_;
    mutable std::vector<std::int32_t> substituteTo_;
    mutable bool parsed_ = false;
};

/// A registry the older version has built into it, by name.
///
/// Biomes below 1.16.2 are what this exists for. From 1.16.2 the server sends its own numbering in
/// the login codec and a chunk carries those numbers; a client older than that has the list compiled
/// in and reads the same numbers against its own. So the ids have to travel through the names, which
/// is the only thing the two sides agree on.
class NameIdMap {
public:
    static constexpr std::int32_t kNone = -1;

    explicit NameIdMap(std::string encoded) : encoded_(std::move(encoded)) {}

    [[nodiscard]] bool empty() const;

    /// That version's id for the name, or none when it has no such entry.
    [[nodiscard]] std::int32_t id(std::string_view name) const;

    /// The name of an id, for a version that reads by name what this one numbers.
    [[nodiscard]] std::string_view name(std::int32_t id) const;

    [[nodiscard]] static const NameIdMap& empty_();

private:
    void parse() const;

    std::string encoded_;
    mutable std::vector<std::string> names_;
    mutable std::vector<std::int32_t> ids_;
    mutable bool parsed_ = false;
};

/// A set of names, parsed on first use.
///
/// What a version cannot be told: the registry entries the version above it added and the attribute
/// keys it invented. Neither is a number that can be remapped: an element a client has never heard
/// of is not a smaller world, it is a client that refuses to load the registries at all.
class NameSet {
public:
    explicit NameSet(std::string encoded) : encoded_(std::move(encoded)) {}

    [[nodiscard]] bool empty() const;

    /// Every name in it, for the caller that needs the set itself and not one question about it.
    [[nodiscard]] const std::vector<std::string>& names() const;

    [[nodiscard]] bool contains(std::string_view name) const;

private:
    void parse() const;

    std::string encoded_;
    mutable std::vector<std::string> names_;
    mutable bool parsed_ = false;
};

} // namespace basalt::wire::multiversion
