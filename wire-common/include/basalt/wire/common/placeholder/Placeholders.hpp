#pragma once

#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace basalt::wire::common::placeholder {

/// Unknown means never registered (a typo or missing plugin); Unavailable means real but unanswerable here.
enum class PlaceholderKind : std::uint8_t { Text, Unavailable, Unknown };

struct PlaceholderValue {
    PlaceholderKind kind = PlaceholderKind::Unknown;
    std::string text;
    std::optional<std::string> reason;

    [[nodiscard]] static PlaceholderValue of(std::string value);
    [[nodiscard]] static PlaceholderValue of(std::int64_t value);
    [[nodiscard]] static PlaceholderValue of(double value);
    [[nodiscard]] static PlaceholderValue of(bool value);
    [[nodiscard]] static PlaceholderValue unavailable(std::optional<std::string> reason = std::nullopt);
    [[nodiscard]] static PlaceholderValue unknown();

    [[nodiscard]] bool isText() const noexcept { return kind == PlaceholderKind::Text; }

    [[nodiscard]] const std::string* textOrNull() const noexcept { return isText() ? &text : nullptr; }

    friend bool operator==(const PlaceholderValue&, const PlaceholderValue&) = default;
};

template <typename T>
class PlaceholderSlot {
public:
    explicit PlaceholderSlot(std::string name) : name_(std::move(name)) {}

    PlaceholderSlot(const PlaceholderSlot&) = delete;
    PlaceholderSlot& operator=(const PlaceholderSlot&) = delete;
    PlaceholderSlot(PlaceholderSlot&&) = delete;
    PlaceholderSlot& operator=(PlaceholderSlot&&) = delete;
    ~PlaceholderSlot() = default;

    [[nodiscard]] const std::string& name() const noexcept { return name_; }

private:
    std::string name_;
};

class PlaceholderContext {
public:
    PlaceholderContext() = default;

    template <typename T>
    [[nodiscard]] const T* get(const PlaceholderSlot<T>& slot) const {
        return static_cast<const T*>(find(&slot));
    }

    template <typename T>
    [[nodiscard]] PlaceholderContext with(const PlaceholderSlot<T>& slot, const T* value) const {
        return value == nullptr ? *this : withRaw(&slot, value);
    }

    [[nodiscard]] std::size_t size() const noexcept { return values_.size(); }

private:
    [[nodiscard]] const void* find(const void* slot) const;
    [[nodiscard]] PlaceholderContext withRaw(const void* slot, const void* value) const;

    std::vector<std::pair<const void*, const void*>> values_;
};

using PlaceholderProvider = std::function<PlaceholderValue(std::string_view params, const PlaceholderContext&)>;

using PlaceholderCatalog = std::function<std::vector<std::string>()>;

class PlaceholderRegistry {
public:
    [[nodiscard]] std::vector<std::string> identifiers() const;

    void registerProvider(std::string_view identifier, PlaceholderProvider provider, PlaceholderCatalog names = {});

    void unregisterProvider(std::string_view identifier);

    [[nodiscard]] std::optional<std::string> identifierFor(std::string_view name) const;

    [[nodiscard]] std::vector<std::string> namesOf(std::string_view identifier) const;

    [[nodiscard]] PlaceholderValue resolve(std::string_view name,
                                           const PlaceholderContext& context = PlaceholderContext{}) const;

private:
    struct Registration {
        std::string identifier;
        PlaceholderProvider provider;
        PlaceholderCatalog names;
    };

    [[nodiscard]] const Registration* registrationFor(std::string_view lowercaseName) const;

    std::vector<Registration> providers_;
};

[[nodiscard]] std::string lowercaseAscii(std::string_view value);

} // namespace basalt::wire::common::placeholder
