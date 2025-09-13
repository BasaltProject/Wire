#pragma once

#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>

namespace basalt::wire::multiversion {

/// One protocol version of Java Edition: the number a client sends in its handshake, and the
/// releases that send it.
///
/// Several releases share a number whenever a release changed nothing on the wire (1.16.4 and 1.16.5
/// are both 754), so the label is a range rather than a single name.
struct ProtocolVersion {
    std::int32_t number = 0;
    std::string_view label;

    [[nodiscard]] std::string toString() const;

    friend bool operator==(const ProtocolVersion&, const ProtocolVersion&) = default;
};

/// Every Java protocol from 1.8 to the native version, and what a number outside the table means.
///
/// Only netty protocols are here. 1.7.x and earlier spoke a different framing altogether and their
/// numbers collide with the modern ones (1.4.2 is also 47), so the floor of the table is where the
/// numbers start being comparable at all.
namespace protocolVersions {

/// The version this build speaks without translating anything.
inline constexpr std::int32_t kNative = 776;

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

/// A snapshot sets bit 30 of its protocol number, and what is left is a counter that has been
/// climbing since 15w32a rather than anything comparable to a release number.
///
/// parity: SharedConstants.SNAPSHOT_PROTOCOL_BIT.
inline constexpr std::int32_t kSnapshotBit = 1 << 30;

/// Oldest first, which is the order a chain of translators runs in.
[[nodiscard]] std::span<const ProtocolVersion> known() noexcept;

[[nodiscard]] std::int32_t oldest() noexcept;

[[nodiscard]] std::optional<ProtocolVersion> of(std::int32_t number) noexcept;

/// What to call a number in a log line, whether or not the table knows it.
[[nodiscard]] std::string describe(std::int32_t number);

[[nodiscard]] constexpr bool isSnapshot(std::int32_t number) noexcept { return (number & kSnapshotBit) != 0; }

/// Whether the number is a release newer than what this build speaks natively.
[[nodiscard]] constexpr bool isFuture(std::int32_t number) noexcept { return !isSnapshot(number) && number > kNative; }

/// Older than the floor of the table: 1.7.x and the pre-netty protocols.
[[nodiscard]] bool isBelowFloor(std::int32_t number) noexcept;

} // namespace protocolVersions

} // namespace basalt::wire::multiversion
