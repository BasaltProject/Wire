#pragma once

#include <compare>
#include <expected>
#include <string>
#include <string_view>

namespace basalt::wire::common {

/// Why a string failed to parse as `namespace:path`.
enum class IdentifierError : std::uint8_t {
    TooManySeparators,
    EmptyPart,
};

/// A namespaced key (`minecraft:stone`, `basalt:some_plugin_block`), the vanilla resource ID format.
class Identifier {
public:
    static constexpr std::string_view kVanillaNamespace = "minecraft";

    Identifier(std::string namespaceName, std::string path)
        : namespace_(std::move(namespaceName)), path_(std::move(path)) {}

    [[nodiscard]] static Identifier vanilla(std::string path) {
        return Identifier{std::string{kVanillaNamespace}, std::move(path)};
    }

    /// Parses `namespace:path`, or bare `path` (defaulting to the vanilla namespace).
    [[nodiscard]] static std::expected<Identifier, IdentifierError> parse(std::string_view value);

    [[nodiscard]] const std::string& namespaceName() const noexcept { return namespace_; }

    [[nodiscard]] const std::string& path() const noexcept { return path_; }

    [[nodiscard]] std::string toString() const { return namespace_ + ':' + path_; }

    friend bool operator==(const Identifier&, const Identifier&) = default;

    /// Namespace first, then path. A total order so an identifier can key an ordered container, and
    /// the order tags and command completions come out in.
    friend std::strong_ordering operator<=>(const Identifier&, const Identifier&) = default;

private:
    std::string namespace_;
    std::string path_;
};

} // namespace basalt::wire::common

template <>
struct std::hash<basalt::wire::common::Identifier> {
    std::size_t operator()(const basalt::wire::common::Identifier& id) const noexcept {
        const std::size_t a = std::hash<std::string>{}(id.namespaceName());
        const std::size_t b = std::hash<std::string>{}(id.path());
        return a ^ (b + 0x9e3779b97f4a7c15ULL + (a << 6) + (a >> 2));
    }
};
