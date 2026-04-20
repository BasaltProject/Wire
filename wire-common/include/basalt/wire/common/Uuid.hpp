#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

namespace basalt::wire::common {

class Uuid {
public:
    constexpr Uuid() = default;

    constexpr Uuid(std::uint64_t high, std::uint64_t low) noexcept : high_(high), low_(low) {}

    [[nodiscard]] constexpr std::uint64_t high() const noexcept { return high_; }

    [[nodiscard]] constexpr std::uint64_t low() const noexcept { return low_; }

    [[nodiscard]] constexpr bool isNil() const noexcept { return high_ == 0 && low_ == 0; }

    [[nodiscard]] std::string toString() const;

    [[nodiscard]] std::string toUndashedString() const;

    [[nodiscard]] static std::optional<Uuid> parse(std::string_view text);

    /// parity: UUID.nameUUIDFromBytes over `OfflinePlayer:<name>`, a version 3 (MD5) UUID.
    [[nodiscard]] static Uuid offlinePlayer(std::string_view name);

    /// parity: java.util.UUID.nameUUIDFromBytes, an MD5 digest stamped with version 3 and variant 2.
    [[nodiscard]] static Uuid nameBased(std::string_view input);

    /// parity: java.util.UUID.randomUUID, random bytes stamped version 4 and variant 2.
    [[nodiscard]] static Uuid random();

    friend constexpr bool operator==(const Uuid&, const Uuid&) = default;

private:
    std::uint64_t high_ = 0;
    std::uint64_t low_ = 0;
};

} // namespace basalt::wire::common

template <>
struct std::hash<basalt::wire::common::Uuid> {
    std::size_t operator()(const basalt::wire::common::Uuid& uuid) const noexcept {
        return static_cast<std::size_t>(uuid.high() ^ (uuid.low() * 0x9e3779b97f4a7c15ULL));
    }
};
