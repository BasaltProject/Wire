#pragma once

#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace basalt::wire::common::placeholder {

/// What a placeholder resolved to, or why it did not.
///
/// The two failures are kept apart because they are different mistakes and deserve different
/// messages: `Unknown` means nobody has ever registered that name, which is a typo or a plugin that
/// is not installed, while `Unavailable` means the name is real but this particular situation
/// cannot answer it, such as asking for a player's coordinates while answering a server list ping,
/// where there is no player yet.
enum class PlaceholderKind : std::uint8_t { Text, Unavailable, Unknown };

struct PlaceholderValue {
    PlaceholderKind kind = PlaceholderKind::Unknown;
    std::string text;
    /// What is missing when the answer depends on something unwritten, so the warning can be
    /// "needs the inventory system" instead of a shrug.
    std::optional<std::string> reason;

    [[nodiscard]] static PlaceholderValue of(std::string value);
    [[nodiscard]] static PlaceholderValue of(std::int64_t value);
    [[nodiscard]] static PlaceholderValue of(double value);
    [[nodiscard]] static PlaceholderValue of(bool value);
    [[nodiscard]] static PlaceholderValue unavailable(std::optional<std::string> reason = std::nullopt);
    [[nodiscard]] static PlaceholderValue unknown();

    [[nodiscard]] bool isText() const noexcept { return kind == PlaceholderKind::Text; }

    /// The text, or nothing when this is one of the two failures.
    [[nodiscard]] const std::string* textOrNull() const noexcept { return isText() ? &text : nullptr; }

    friend bool operator==(const PlaceholderValue&, const PlaceholderValue&) = default;
};

/// A typed key for one thing a context can carry, such as "the player this is about". Contexts are
/// built out of these rather than a fixed set of fields so the registry can live below every module
/// that has something to put in one.
///
/// A slot is its own identity, so it has to outlive every context carrying it: declare one once, next
/// to whatever it names.
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

/// What a placeholder is being resolved against: a player, a world, a pending connection, nothing.
///
/// It borrows rather than owns: a context is built for one resolution and everything in it outlives
/// that.
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

/// Answers every placeholder under one identifier.
///
/// The parameter is what is left after the identifier: `%basalt_server_online%` reaches the
/// provider registered as `basalt_server` with params `online`. That split is what lets one
/// provider serve a whole family, including the ones that carry an argument
/// (`%basalt_player_has_permission_x%`), without registering a name per placeholder.
using PlaceholderProvider = std::function<PlaceholderValue(std::string_view params, const PlaceholderContext&)>;

/// The names a provider is willing to list, for a provider that can enumerate itself.
///
/// A name taking an argument is listed with the argument in angle brackets, e.g.
/// `has_permission_<node>`, since there is no finite list of those to print.
using PlaceholderCatalog = std::function<std::vector<std::string>()>;

/// Every placeholder this server knows. One registry per server rather than a global singleton,
/// which would be reachable from a regional tick.
///
/// Identifiers may contain underscores, so a name matches the longest registered identifier it
/// starts with: that is what keeps `%basalt_server_online%` apart from a plugin registering
/// `basalt`.
class PlaceholderRegistry {
public:
    /// The registered identifiers, in the order they were registered.
    [[nodiscard]] std::vector<std::string> identifiers() const;

    /// Registering an identifier twice replaces it, which is how a plugin reloads its own.
    void registerProvider(std::string_view identifier, PlaceholderProvider provider, PlaceholderCatalog names = {});

    void unregisterProvider(std::string_view identifier);

    /// Which registered identifier serves `name`, if any, so a caller can report or list it.
    [[nodiscard]] std::optional<std::string> identifierFor(std::string_view name) const;

    /// Every name `identifier`'s provider is willing to list, or empty if it does not enumerate
    /// itself.
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

/// ASCII lowercase, which is what a placeholder identifier is made of. Not a locale's idea of it:
/// a Turkish one would turn the `I` of an identifier into something no lookup would find again.
[[nodiscard]] std::string lowercaseAscii(std::string_view value);

} // namespace basalt::wire::common::placeholder
