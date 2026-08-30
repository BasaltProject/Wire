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
    FrameTooBig,
    MalformedVarInt,
    DecompressionFailed,
};

/// The play-protocol wire framing, confirmed against the decompiled 26.2 CompressionEncoder and
/// CompressionDecoder.
///
/// Uncompressed: `VarInt packetLength, [packet id + data]`. Compressed: `VarInt packetLength, VarInt
/// dataLength, payload`, where a `dataLength` of 0 means the packet was under the threshold and
/// otherwise it is the uncompressed size of a zlib-deflated payload.
namespace packetFraming {

inline constexpr std::int32_t kMaxUncompressedSize = 8 * 1024 * 1024;

void writeUncompressed(io::ByteBuffer& output, std::span<const std::uint8_t> packetIdAndData);

/// Compresses straight into the output buffer, with no staging copy of the frame. The old path
/// built the frame, then the compressed body, then the framed result, which is where a chunk packet
/// picked up most of its garbage.
std::expected<void, FramingError> writeCompressed(io::ByteBuffer& output, std::span<const std::uint8_t> packetIdAndData,
                                                  std::int32_t threshold);

/// Extracts one complete frame's `packet id + data` from an accumulating receive buffer into
/// `into`, which is cleared first and reused across calls. Reports false without touching the input
/// when a whole frame is not buffered yet.
std::expected<bool, FramingError> tryReadFrame(io::ByteBuffer& input, bool compressionEnabled, io::ByteBuffer& into);

} // namespace packetFraming

} // namespace basalt::wire::protocol
