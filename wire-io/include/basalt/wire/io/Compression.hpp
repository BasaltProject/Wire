#pragma once

#include <basalt/wire/io/ByteBuffer.hpp>

#include <cstdint>
#include <expected>
#include <span>
#include <vector>

namespace basalt::wire::io {

/// Why a compression call failed.
enum class CompressionError : std::uint8_t {
    /// The destination was too small; a bigger one may work.
    BufferTooSmall,
    /// Corrupt or truncated input, or a stream that claims more than the cap allows.
    CorruptData,
    OutOfMemory,
    /// The declared uncompressed size is beyond what the server will hand out for one message.
    TooLarge,
};

/// Nothing decompresses past this in one call: a crafted packet must not be able to ask for a
/// gigabyte of buffer.
inline constexpr std::size_t kMaxDecompressedSize = 512UL * 1024UL * 1024UL;

/// zlib-wrapped deflate (RFC 1950), the format the play protocol uses for packet bodies.
namespace zlib {

/// Compresses straight into the destination buffer's tail. Nothing is allocated on the way: the
/// old path made five full copies per chunk packet, which was a gigabyte of garbage per minute of
/// play, and that is the reason this signature takes a buffer rather than returning one.
std::expected<void, CompressionError> compressInto(std::span<const std::uint8_t> source, ByteBuffer& destination,
                                                   int level = 6);

/// Decompresses into the destination buffer's tail, given the exact size the protocol declares.
std::expected<void, CompressionError> decompressInto(std::span<const std::uint8_t> source, std::size_t uncompressedSize,
                                                     ByteBuffer& destination);

[[nodiscard]] std::expected<std::vector<std::uint8_t>, CompressionError> compress(std::span<const std::uint8_t> source,
                                                                                  int level = 6);

[[nodiscard]] std::expected<std::vector<std::uint8_t>, CompressionError>
decompress(std::span<const std::uint8_t> source, std::size_t uncompressedSize);

/// Decompresses without knowing the output size ahead of time, growing a scratch buffer.
[[nodiscard]] std::expected<std::vector<std::uint8_t>, CompressionError>
decompressUnknownSize(std::span<const std::uint8_t> source);

} // namespace zlib

/// gzip (RFC 1952), the wrapper saved structure files use and rotated logs are archived with. The
/// uncompressed size does not have to be known: the format's own trailer stores it.
namespace gzip {

[[nodiscard]] std::expected<std::vector<std::uint8_t>, CompressionError> compress(std::span<const std::uint8_t> source,
                                                                                  int level = 6);

[[nodiscard]] std::expected<std::vector<std::uint8_t>, CompressionError>
decompress(std::span<const std::uint8_t> source);

} // namespace gzip

/// Bare DEFLATE (RFC 1951), no wrapper at all. What a ZIP entry stores.
namespace rawDeflate {

[[nodiscard]] std::expected<std::vector<std::uint8_t>, CompressionError>
decompress(std::span<const std::uint8_t> source, std::size_t uncompressedSize);

} // namespace rawDeflate

} // namespace basalt::wire::io
