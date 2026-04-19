#pragma once

#include <basalt/wire/io/ByteBuffer.hpp>

#include <cstdint>
#include <expected>
#include <optional>
#include <span>

namespace basalt::wire::protocol {

/// Why a frame could not be read.
enum class FramingError : std::uint8_t {
    NegativeLength,
    EmptyFrame,
    BelowCompressionThreshold,
    FrameTooBig,
    MalformedVarInt,
    DecompressionFailed,
};

/// The play-protocol wire framing, confirmed against the decompiled 26.3 CompressionEncoder and
/// CompressionDecoder.
///
/// Uncompressed: `VarInt packetLength, [packet id + data]`. Compressed: `VarInt packetLength, VarInt
/// dataLength, payload`, where a `dataLength` of 0 means the packet was under the threshold and
/// otherwise it is the uncompressed size of a zlib-deflated payload.
namespace packetFraming {

inline constexpr std::int32_t kMaxFrameSize = (1 << 21) - 1;
inline constexpr std::int32_t kMaxUncompressedSize = 8 * 1024 * 1024;

struct FrameSizeHint {
    std::size_t frameBytes;
    std::optional<std::size_t> packetBytes;
    bool complete;
};

/// Inspects available headers without consuming, allocating or inflating. Compression headers
/// cannot read outside their declared frame, even when a following frame is already buffered.
[[nodiscard]] std::expected<std::optional<FrameSizeHint>, FramingError>
inspectFrame(std::span<const std::uint8_t> bytes, bool compressed, std::size_t maxPacketBytes = kMaxUncompressedSize,
             std::optional<std::int32_t> threshold = std::nullopt);

[[nodiscard]] std::expected<void, FramingError> writeUncompressed(io::ByteBuffer& output,
                                                                  std::span<const std::uint8_t> packetIdAndData);

/// Compresses straight into the output buffer, with no staging copy of the frame. The old path
/// built the frame, then the compressed body, then the framed result, which is where a chunk packet
/// picked up most of its garbage.
std::expected<void, FramingError> writeCompressed(io::ByteBuffer& output, std::span<const std::uint8_t> packetIdAndData,
                                                  std::int32_t threshold);

/// Extracts one complete frame's `packet id + data` from an accumulating receive buffer into
/// `into`, which is cleared first and reused across calls. Reports false without touching the input
/// when a whole frame is not buffered yet.
/// A supplied threshold validates compressed packets against the negotiated threshold.
std::expected<bool, FramingError> tryReadFrame(io::ByteBuffer& input, bool compressionEnabled, io::ByteBuffer& into,
                                               std::optional<std::int32_t> threshold = std::nullopt);

} // namespace packetFraming

} // namespace basalt::wire::protocol
