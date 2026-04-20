#pragma once

#include <cstdint>

namespace basalt::wire::protocol::play {

enum class Direction : std::uint8_t {
    Down,
    Up,
    North,
    South,
    West,
    East,
};

/// parity: Direction.getUnitVec3i.
[[nodiscard]] constexpr std::int32_t normalX(Direction direction) noexcept {
    return direction == Direction::West ? -1 : (direction == Direction::East ? 1 : 0);
}

[[nodiscard]] constexpr std::int32_t normalY(Direction direction) noexcept {
    return direction == Direction::Down ? -1 : (direction == Direction::Up ? 1 : 0);
}

[[nodiscard]] constexpr std::int32_t normalZ(Direction direction) noexcept {
    return direction == Direction::North ? -1 : (direction == Direction::South ? 1 : 0);
}

/// parity: Pose.
enum class Pose : std::uint8_t {
    Standing,
    FallFlying,
    Sleeping,
    Swimming,
    SpinAttack,
    Crouching,
    LongJumping,
    Dying,
    Croaking,
    UsingTongue,
    Sitting,
    Roaring,
    Sniffing,
    Emerging,
    Digging,
    Sliding,
    Shooting,
    Inhaling,
};

/// A block position, packed the way vanilla's BlockPos.asLong does it: 26 bits of x, 26 of z, 12 of y.
struct BlockPos {
    std::int32_t x = 0;
    std::int32_t y = 0;
    std::int32_t z = 0;

    [[nodiscard]] constexpr std::int64_t pack() const noexcept {
        const auto bits = ((static_cast<std::uint64_t>(x) & 0x3FFFFFFULL) << 38) |
                          ((static_cast<std::uint64_t>(z) & 0x3FFFFFFULL) << 12) |
                          (static_cast<std::uint64_t>(y) & 0xFFFULL);
        return static_cast<std::int64_t>(bits);
    }

    [[nodiscard]] static constexpr BlockPos unpack(std::int64_t packed) noexcept {
        // Shifting left then arithmetically right is how vanilla sign-extends each field.
        const auto bits = static_cast<std::uint64_t>(packed);
        const auto sign = [](std::uint64_t value, int left, int right) {
            return static_cast<std::int32_t>(static_cast<std::int64_t>(value << left) >> right);
        };
        return BlockPos{sign(bits, 0, 38), sign(bits, 52, 52), sign(bits, 26, 38)};
    }

    /// parity: BlockPos.relative.
    [[nodiscard]] constexpr BlockPos offset(Direction direction) const noexcept {
        return BlockPos{x + normalX(direction), y + normalY(direction), z + normalZ(direction)};
    }

    friend constexpr bool operator==(const BlockPos&, const BlockPos&) = default;
};

} // namespace basalt::wire::protocol::play
