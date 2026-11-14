#pragma once

#include <atomic>
#include <cstdint>
#include <mutex>
#include <string>
#include <string_view>
#include <vector>

namespace basalt::wire::multiversion {

/// Whether a table shared by every session has been parsed: several I/O threads can reach it first at once.
class ParsedFlag {
public:
    ParsedFlag() = default;

    ParsedFlag(const ParsedFlag& other) noexcept : value_(other.value_.load(std::memory_order_acquire)) {}

    ParsedFlag& operator=(const ParsedFlag& other) noexcept {
        value_.store(other.value_.load(std::memory_order_acquire), std::memory_order_release);
        return *this;
    }

    ~ParsedFlag() = default;

    explicit operator bool() const noexcept { return value_.load(std::memory_order_acquire); }

    ParsedFlag& operator=(bool value) noexcept {
        value_.store(value, std::memory_order_release);
        return *this;
    }

    /// Held while parsing, so two threads never fill the same table.
    [[nodiscard]] static std::mutex& lock() noexcept;

private:
    std::atomic<bool> value_{false};
};

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

    /// The inverse, with the older ids no run reaches sent where `preferred` says.
    [[nodiscard]] IdMap inverted(const IdMap& preferred) const;

private:
    void parse() const;

    std::string encoded_;
    mutable std::vector<std::int32_t> starts_;
    mutable std::vector<std::int32_t> lengths_;
    mutable std::vector<std::int32_t> deltas_;
    mutable std::vector<std::int32_t> substituteFrom_;
    mutable std::vector<std::int32_t> substituteTo_;
    mutable ParsedFlag parsed_;
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
    mutable ParsedFlag parsed_;
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
    mutable ParsedFlag parsed_;
};

} // namespace basalt::wire::multiversion
