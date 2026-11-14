#pragma once

#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace basalt::wire::permissions {

enum class Tristate : std::uint8_t { kUndefined, kFalse, kTrue };

[[nodiscard]] constexpr Tristate tristateOf(bool value) noexcept { return value ? Tristate::kTrue : Tristate::kFalse; }

/// The product's own answer (groups, ops); always definite and asked last.
inline constexpr std::int32_t kPriorityDefault = 0;
inline constexpr std::int32_t kPriorityPlugin = 100;
inline constexpr std::int32_t kPriorityAdapter = 200;

inline constexpr std::string_view kContextServer = "server";
inline constexpr std::string_view kContextWorld = "world";

enum class SubjectKind : std::uint8_t { kPlayer, kConsole };

/// Who is asking; views that only need to live for the duration of one check.
struct Subject {
    SubjectKind kind = SubjectKind::kPlayer;
    std::string_view uuid;
    std::string_view name;

    [[nodiscard]] static constexpr Subject console() noexcept { return {SubjectKind::kConsole, {}, "console"}; }

    [[nodiscard]] static constexpr Subject player(std::string_view uuid, std::string_view name) noexcept {
        return {SubjectKind::kPlayer, uuid, name};
    }

    [[nodiscard]] constexpr bool isConsole() const noexcept { return kind == SubjectKind::kConsole; }
};

struct ContextEntry {
    std::string_view key;
    std::string_view value;
};

struct Context {
    std::string_view server;
    std::string_view world;
    std::span<const ContextEntry> extra;

    /// Empty when the context has no such key.
    [[nodiscard]] std::string_view value(std::string_view key) const noexcept;
};

struct Provider;

/// One provider's view of a check; rest() is what the providers below it would answer.
class Query {
public:
    [[nodiscard]] const Subject& subject() const noexcept { return *subject_; }

    [[nodiscard]] std::string_view node() const noexcept { return node_; }

    [[nodiscard]] const Context& context() const noexcept { return *context_; }

    [[nodiscard]] Tristate rest() const;

private:
    friend class PermissionChain;

    Query(std::span<const std::shared_ptr<const Provider>> below, const Subject& subject, std::string_view node,
          const Context& context) noexcept
        : below_(below), subject_(&subject), node_(node), context_(&context) {}

    std::span<const std::shared_ptr<const Provider>> below_;
    const Subject* subject_;
    std::string_view node_;
    const Context* context_;
};

/// Undefined from check() lets the chain go on; meta may stay empty.
struct Provider {
    std::string id;
    std::int32_t priority = kPriorityPlugin;
    std::function<Tristate(const Query&)> check;
    std::function<std::optional<std::string>(const Subject&, std::string_view key, const Context&)> meta;
};

class ProviderHandle {
public:
    ProviderHandle() = default;
    ProviderHandle(ProviderHandle&& other) noexcept;
    ProviderHandle& operator=(ProviderHandle&& other) noexcept;
    ProviderHandle(const ProviderHandle&) = delete;
    ProviderHandle& operator=(const ProviderHandle&) = delete;
    ~ProviderHandle();

    /// Takes the provider out of its chain; safe after the chain is gone.
    void remove();

    [[nodiscard]] bool active() const noexcept { return !owner_.expired(); }

private:
    friend class PermissionChain;

    ProviderHandle(std::weak_ptr<void> owner, std::shared_ptr<const Provider> provider) noexcept
        : owner_(std::move(owner)), provider_(std::move(provider)) {}

    std::weak_ptr<void> owner_;
    std::shared_ptr<const Provider> provider_;
};

/// Providers by descending priority, then by registration; the first definite answer wins. Safe from any thread.
class PermissionChain {
public:
    PermissionChain();
    ~PermissionChain();
    PermissionChain(const PermissionChain&) = delete;
    PermissionChain& operator=(const PermissionChain&) = delete;

    [[nodiscard]] ProviderHandle provide(Provider provider);

    [[nodiscard]] Tristate check(const Subject& subject, std::string_view node, const Context& context = {}) const;

    /// Undefined is denied.
    [[nodiscard]] bool has(const Subject& subject, std::string_view node, const Context& context = {}) const {
        return check(subject, node, context) == Tristate::kTrue;
    }

    [[nodiscard]] std::optional<std::string> meta(const Subject& subject, std::string_view key,
                                                  const Context& context = {}) const;

    /// True when nothing above the default priority is registered, so the caller may skip building a context.
    [[nodiscard]] bool onlyDefault() const noexcept;

    /// Provider ids in the order they are asked.
    [[nodiscard]] std::vector<std::string> ids() const;

    /// A provider's answers changed; bumps version() so derived state can be rebuilt.
    void changed() noexcept;

    [[nodiscard]] std::uint64_t version() const noexcept;

private:
    friend class Query;
    friend class ProviderHandle;
    struct State;

    [[nodiscard]] static Tristate walk(std::span<const std::shared_ptr<const Provider>> providers,
                                       const Subject& subject, std::string_view node, const Context& context);

    std::shared_ptr<State> state_;
};

} // namespace basalt::wire::permissions
