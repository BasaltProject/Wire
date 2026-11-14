#pragma once

#include <basalt/wire/multiversion/ProtocolVersions.hpp>

#include <algorithm>
#include <array>
#include <cstdint>

namespace basalt::wire::multiversion::java {

/// parity: the 26.3 entity types whose class extends LivingEntity, the ones a version before 1.19 spawned as mobs.
inline constexpr std::array<std::int32_t, 92> kNativeLiving = {
    2,   4,   5,   7,   10,  11,  14,  16,  17,  19,  20,  21,  22,  26,  27,  28,  30,  31,  32,  36,  37,  39,  41,
    42,  43,  44,  47,  55,  56,  58,  59,  60,  62,  63,  64,  65,  67,  68,  69,  71,  79,  81,  84,  87,  88,  89,
    92,  97,  98,  99,  100, 101, 102, 103, 104, 105, 110, 111, 112, 113, 114, 115, 117, 118, 119, 120, 122, 124, 127,
    130, 131, 132, 133, 134, 138, 140, 141, 142, 143, 144, 145, 146, 148, 149, 150, 152, 153, 154, 155, 156, 157, 158};

inline constexpr std::int32_t kArmorStand = 5;
inline constexpr std::int32_t kMannequin = 84;

/// Whether a far end spawns this type as a mob: before 1.14 an armor stand, and what stands in for one, was an object.
[[nodiscard]] constexpr bool nativeLiving(std::int32_t type, std::int32_t farEnd) noexcept {
    if (farEnd < protocolVersions::kV1_14 && (type == kArmorStand || type == kMannequin)) {
        return false;
    }
    return std::ranges::binary_search(kNativeLiving, type);
}

} // namespace basalt::wire::multiversion::java
