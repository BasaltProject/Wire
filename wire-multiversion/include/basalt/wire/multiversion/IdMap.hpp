#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace basalt::wire::multiversion {

class IdLookup {
public:
    static constexpr std::int32_t kNone = -1;

    IdLookup() = default;
    IdLookup(const IdLookup&) = default;
    IdLookup(IdLookup&&) = default;
    IdLookup& operator=(const IdLookup&) = default;
    IdLookup& operator=(IdLookup&&) = default;
    virtual ~IdLookup() = default;

    [[nodiscard]] virtual std::int32_t map(std::int32_t id) const = 0;

    [[nodiscard]] virtual std::int32_t unmap(std::int32_t id) const = 0;

    [[nodiscard]] virtual bool substitutes(std::int32_t id) const = 0;
};

/// Parsed on first use, so a link nobody needs costs nothing.
class IdMap final : public IdLookup {
public:
    explicit IdMap(std::string encoded) : encoded_(std::move(encoded)) {}

    [[nodiscard]] std::int32_t map(std::int32_t id) const override;

    [[nodiscard]] std::int32_t unmap(std::int32_t id) const override;

    [[nodiscard]] bool substitutes(std::int32_t id) const override;

    [[nodiscard]] static const IdMap& identity();

    [[nodiscard]] IdMap inverted() const;

private:
    void parse() const;

    std::string encoded_;
    mutable std::vector<std::int32_t> starts_;
    mutable std::vector<std::int32_t> lengths_;
    mutable std::vector<std::int32_t> deltas_;
    mutable std::vector<std::int32_t> substituteFrom_;
    mutable std::vector<std::int32_t> substituteTo_;
    mutable bool parsed_ = false;
};

class NameIdMap {
public:
    static constexpr std::int32_t kNone = -1;

    explicit NameIdMap(std::string encoded) : encoded_(std::move(encoded)) {}

    [[nodiscard]] bool empty() const;

    [[nodiscard]] std::int32_t id(std::string_view name) const;

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
class NameSet {
public:
    explicit NameSet(std::string encoded) : encoded_(std::move(encoded)) {}

    [[nodiscard]] bool empty() const;

    [[nodiscard]] const std::vector<std::string>& names() const;

    [[nodiscard]] bool contains(std::string_view name) const;

private:
    void parse() const;

    std::string encoded_;
    mutable std::vector<std::string> names_;
    mutable bool parsed_ = false;
};

} // namespace basalt::wire::multiversion
