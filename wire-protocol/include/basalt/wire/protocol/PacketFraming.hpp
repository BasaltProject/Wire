#pragma once

#include <basalt/wire/io/ByteBuffer.hpp>

#include <cstdint>
#include <expected>
#include <optional>
#include <span>

namespace basalt::wire::protocol {

enum class FramingError : std::uint8_t {
    NegativeLength,
    EmptyFrame,
    BelowCompressionThreshold,
    FrameTooBig,
    MalformedVarInt,
    DecompressionFailed,
};

namespace packetFraming {

inline constexpr std::int32_t kMaxFrameSize = (1 << 21) - 1;
inline constexpr std::int32_t kMaxUncompressedSize = 8 * 1024 * 1024;

struct FrameSizeHint {
    std::size_t frameBytes;
    std::optional<std::size_t> packetBytes;
    bool complete;
};

/// Inspects available headers without consuming, allocating or inflating.
[[nodiscard]] std::expected<std::optional<FrameSizeHint>, FramingError>
inspectFrame(std::span<const std::uint8_t> bytes, bool compressed, std::size_t maxPacketBytes = kMaxUncompressedSize,
             std::optional<std::int32_t> threshold = std::nullopt);

[[nodiscard]] std::expected<void, FramingError> writeUncompressed(io::ByteBuffer& output,
                                                                  std::span<const std::uint8_t> packetIdAndData);

std::expected<void, FramingError> writeCompressed(io::ByteBuffer& output, std::span<const std::uint8_t> packetIdAndData,
                                                  std::int32_t threshold, int zlibLevel = 6);

/// Reports false without touching the input when a whole frame is not buffered yet.
std::expected<bool, FramingError> tryReadFrame(io::ByteBuffer& input, bool compressionEnabled, io::ByteBuffer& into,
                                               std::optional<std::int32_t> threshold = std::nullopt);

} // namespace packetFraming

} // namespace basalt::wire::protocol
