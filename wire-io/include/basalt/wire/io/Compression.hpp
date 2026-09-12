#pragma once

#include <basalt/wire/io/ByteBuffer.hpp>

#include <cstdint>
#include <expected>
#include <functional>
#include <memory>
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

/// Bare DEFLATE (RFC 1951), no wrapper at all. What a ZIP entry stores, and what a Bedrock batch is
/// compressed with: that protocol never sends the uncompressed size, so its reader is the one that
/// has to grow.
namespace rawDeflate {

[[nodiscard]] std::expected<std::vector<std::uint8_t>, CompressionError>
decompress(std::span<const std::uint8_t> source, std::size_t uncompressedSize);

[[nodiscard]] std::expected<std::vector<std::uint8_t>, CompressionError>
decompressUnknownSize(std::span<const std::uint8_t> source);

[[nodiscard]] std::expected<std::vector<std::uint8_t>, CompressionError> compress(std::span<const std::uint8_t> source,
                                                                                  int level = 6);

} // namespace rawDeflate

/// zstd (RFC 8878), the compression the Linear region format wraps a whole region in, once per file,
/// unlike Anvil's per-chunk zlib. That single stream is where the format's 50 % on the Overworld and
/// 95 % on the End come from: compressing the region whole gives the compressor real context.
namespace zstd {

inline constexpr int kDefaultLevel = 3;

[[nodiscard]] std::expected<std::vector<std::uint8_t>, CompressionError> compress(std::span<const std::uint8_t> source,
                                                                                  int level = kDefaultLevel);

/// The uncompressed size is read from the frame header when it is there, and the buffer grows when
/// it is not.
[[nodiscard]] std::expected<std::vector<std::uint8_t>, CompressionError>
decompress(std::span<const std::uint8_t> source);

/// Compresses a sequence of pieces as one stream, handing each block of output to the sink as it is
/// produced. Never holds the input or the output whole.
///
/// This is what lets a caller compress something far larger than anything it allocates: a Linear
/// region is written from the chunk blobs it already holds, straight to the file, instead of
/// concatenating tens of megabytes and then compressing that.
class StreamCompressor {
public:
    using Sink = std::function<void(std::span<const std::uint8_t>)>;

    explicit StreamCompressor(Sink sink, int level = kDefaultLevel);
    StreamCompressor(const StreamCompressor&) = delete;
    StreamCompressor& operator=(const StreamCompressor&) = delete;
    ~StreamCompressor();

    [[nodiscard]] bool valid() const noexcept;

    /// Feeds one piece. False once the stream has failed.
    bool feed(std::span<const std::uint8_t> piece);

    /// Flushes the tail of the stream. The compressor is finished afterwards.
    bool finish();

private:
    struct State;

    std::unique_ptr<State> state_;
};

/// Reads a zstd frame piece by piece, so a caller that knows how the decompressed bytes are laid
/// out never holds them all.
///
/// This is the reading half of what `StreamCompressor` does: a Linear region decompresses to its
/// chunk table followed by the chunk blobs, and a reader that wants one chunk out of it has no
/// reason to materialise the other thousand. Decompressing whole regions per read was megabytes of
/// transient on every worker thread, and each of their allocators kept the high-water mark.
class StreamDecompressor {
public:
    explicit StreamDecompressor(std::span<const std::uint8_t> source);
    /// The reader borrows the bytes rather than copying them, so a temporary handed to it would be
    /// gone before the first read. Refusing it here turns that into a compile error.
    explicit StreamDecompressor(std::vector<std::uint8_t>&&) = delete;
    StreamDecompressor(const StreamDecompressor&) = delete;
    StreamDecompressor& operator=(const StreamDecompressor&) = delete;
    ~StreamDecompressor();

    [[nodiscard]] bool valid() const noexcept;

    /// Fills `destination` completely. False when the frame ended first or went wrong, and the
    /// stream is finished either way.
    bool readFully(std::span<std::uint8_t> destination);

    /// Whether the frame ended exactly where the reader stopped, with nothing left undelivered.
    /// What tells a format its own table was right about how much there would be.
    [[nodiscard]] bool frameComplete() const noexcept;

private:
    struct State;

    std::unique_ptr<State> state_;
};

} // namespace zstd

} // namespace basalt::wire::io
