#pragma once

#include <basalt/wire/io/ByteBuffer.hpp>

#include <cstdint>

namespace basalt::wire::protocol {

/// parity: net.minecraft.network.VarInt and VarLong.
namespace varInt {

inline constexpr int kMaxBytes = 5;

[[nodiscard]] constexpr int sizeOf(std::int32_t value) noexcept {
    for (int i = 1; i < 5; ++i) {
        if ((value & (-1 << (i * 7))) == 0) {
            return i;
        }
    }
    return 5;
}

inline void write(io::ByteBuffer& buffer, std::int32_t value) {
    auto bits = static_cast<std::uint32_t>(value);
    while ((bits & 0xFFFFFF80U) != 0) {
        buffer.writeByte(static_cast<std::uint8_t>((bits & 0x7FU) | 0x80U));
        bits >>= 7;
    }
    buffer.writeByte(static_cast<std::uint8_t>(bits));
}

[[nodiscard]] inline std::int32_t read(io::ByteBuffer& buffer) noexcept {
    std::uint32_t result = 0;
    for (int i = 0; i < kMaxBytes; ++i) {
        const std::uint8_t current = buffer.readByte();
        if (buffer.failed()) {
            return 0;
        }
        result |= static_cast<std::uint32_t>(current & 0x7FU) << (i * 7);
        if ((current & 0x80U) == 0) {
            return static_cast<std::int32_t>(result);
        }
    }
    buffer.markFailed();
    return 0;
}

} // namespace varInt

namespace varLong {

inline constexpr int kMaxBytes = 10;

inline void write(io::ByteBuffer& buffer, std::int64_t value) {
    auto bits = static_cast<std::uint64_t>(value);
    while ((bits & ~0x7FULL) != 0) {
        buffer.writeByte(static_cast<std::uint8_t>((bits & 0x7FU) | 0x80U));
        bits >>= 7;
    }
    buffer.writeByte(static_cast<std::uint8_t>(bits));
}

[[nodiscard]] inline std::int64_t read(io::ByteBuffer& buffer) noexcept {
    std::uint64_t result = 0;
    for (int i = 0; i < kMaxBytes; ++i) {
        const std::uint8_t current = buffer.readByte();
        if (buffer.failed()) {
            return 0;
        }
        result |= static_cast<std::uint64_t>(current & 0x7FU) << (i * 7);
        if ((current & 0x80U) == 0) {
            return static_cast<std::int64_t>(result);
        }
    }
    buffer.markFailed();
    return 0;
}

} // namespace varLong

} // namespace basalt::wire::protocol
