#pragma once

#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>

namespace basalt::wire::multiversion {

struct ProtocolVersion {
    std::int32_t number = 0;
    std::string_view label;

    [[nodiscard]] std::string toString() const;

    friend bool operator==(const ProtocolVersion&, const ProtocolVersion&) = default;
};

/// Only netty protocols are here.
namespace protocolVersions {

/// The version this build speaks without translating anything.
inline constexpr std::int32_t kNative = 777;

inline constexpr std::int32_t kV1_8 = 47;
inline constexpr std::int32_t kV1_9 = 107;
inline constexpr std::int32_t kV1_9_1 = 108;
inline constexpr std::int32_t kV1_9_2 = 109;
inline constexpr std::int32_t kV1_9_4 = 110;
inline constexpr std::int32_t kV1_10 = 210;
inline constexpr std::int32_t kV1_11 = 315;
inline constexpr std::int32_t kV1_11_1 = 316;
inline constexpr std::int32_t kV1_12 = 335;
inline constexpr std::int32_t kV1_12_1 = 338;
inline constexpr std::int32_t kV1_12_2 = 340;
inline constexpr std::int32_t kV1_13 = 393;
inline constexpr std::int32_t kV1_13_1 = 401;
inline constexpr std::int32_t kV1_13_2 = 404;
inline constexpr std::int32_t kV1_14 = 477;
inline constexpr std::int32_t kV1_14_1 = 480;
inline constexpr std::int32_t kV1_14_2 = 485;
inline constexpr std::int32_t kV1_14_3 = 490;
inline constexpr std::int32_t kV1_14_4 = 498;
inline constexpr std::int32_t kV1_15 = 573;
inline constexpr std::int32_t kV1_15_1 = 575;
inline constexpr std::int32_t kV1_15_2 = 578;
inline constexpr std::int32_t kV1_16 = 735;
inline constexpr std::int32_t kV1_16_1 = 736;
inline constexpr std::int32_t kV1_16_2 = 751;
inline constexpr std::int32_t kV1_16_3 = 753;
inline constexpr std::int32_t kV1_16_4 = 754;
inline constexpr std::int32_t kV1_17 = 755;
inline constexpr std::int32_t kV1_17_1 = 756;
inline constexpr std::int32_t kV1_18 = 757;
inline constexpr std::int32_t kV1_18_2 = 758;
inline constexpr std::int32_t kV1_19 = 759;
inline constexpr std::int32_t kV1_19_1 = 760;
inline constexpr std::int32_t kV1_19_3 = 761;
inline constexpr std::int32_t kV1_19_4 = 762;
inline constexpr std::int32_t kV1_20 = 763;
inline constexpr std::int32_t kV1_20_2 = 764;
inline constexpr std::int32_t kV1_20_3 = 765;
inline constexpr std::int32_t kV1_20_5 = 766;
inline constexpr std::int32_t kV1_21 = 767;
inline constexpr std::int32_t kV1_21_2 = 768;
inline constexpr std::int32_t kV1_21_4 = 769;
inline constexpr std::int32_t kV1_21_5 = 770;
inline constexpr std::int32_t kV1_21_6 = 771;
inline constexpr std::int32_t kV1_21_7 = 772;
inline constexpr std::int32_t kV1_21_9 = 773;
inline constexpr std::int32_t kV1_21_11 = 774;
inline constexpr std::int32_t kV26_1 = 775;
inline constexpr std::int32_t kV26_2 = 776;

/// parity: SharedConstants.SNAPSHOT_PROTOCOL_BIT.
inline constexpr std::int32_t kSnapshotBit = 1 << 30;

[[nodiscard]] std::span<const ProtocolVersion> known() noexcept;

[[nodiscard]] std::int32_t oldest() noexcept;

[[nodiscard]] std::optional<ProtocolVersion> of(std::int32_t number) noexcept;

[[nodiscard]] std::string describe(std::int32_t number);

[[nodiscard]] constexpr bool isSnapshot(std::int32_t number) noexcept { return (number & kSnapshotBit) != 0; }

[[nodiscard]] constexpr bool isFuture(std::int32_t number) noexcept { return !isSnapshot(number) && number > kNative; }

[[nodiscard]] bool isBelowFloor(std::int32_t number) noexcept;

} // namespace protocolVersions

} // namespace basalt::wire::multiversion
