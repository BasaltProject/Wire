#pragma once

#include <basalt/wire/io/ByteBuffer.hpp>

#include <cstdint>
#include <expected>
#include <functional>
#include <memory>
#include <span>
#include <vector>

namespace basalt::wire::io {

enum class CompressionError : std::uint8_t {
    BufferTooSmall,
    CorruptData,
    OutOfMemory,
    TooLarge,
};

/// Ceiling per call, so a crafted packet cannot demand a gigabyte of buffer.
inline constexpr std::size_t kMaxDecompressedSize = 512UL * 1024UL * 1024UL;

namespace zlib {

std::expected<void, CompressionError> compressInto(std::span<const std::uint8_t> source, ByteBuffer& destination,
                                                   int level = 6);

std::expected<void, CompressionError> decompressInto(std::span<const std::uint8_t> source, std::size_t uncompressedSize,
                                                     ByteBuffer& destination);

[[nodiscard]] std::expected<std::vector<std::uint8_t>, CompressionError> compress(std::span<const std::uint8_t> source,
                                                                                  int level = 6);

[[nodiscard]] std::expected<std::vector<std::uint8_t>, CompressionError>
decompress(std::span<const std::uint8_t> source, std::size_t uncompressedSize);

[[nodiscard]] std::expected<std::vector<std::uint8_t>, CompressionError>
decompressUnknownSize(std::span<const std::uint8_t> source);

} // namespace zlib

namespace gzip {

[[nodiscard]] std::expected<std::vector<std::uint8_t>, CompressionError> compress(std::span<const std::uint8_t> source,
                                                                                  int level = 6);

[[nodiscard]] std::expected<std::vector<std::uint8_t>, CompressionError>
decompress(std::span<const std::uint8_t> source);

} // namespace gzip

/// Bare DEFLATE (RFC 1951), as in ZIP entries and Bedrock batches, which never send the uncompressed size.
namespace rawDeflate {

[[nodiscard]] std::expected<std::vector<std::uint8_t>, CompressionError>
decompress(std::span<const std::uint8_t> source, std::size_t uncompressedSize);

[[nodiscard]] std::expected<std::vector<std::uint8_t>, CompressionError>
decompressUnknownSize(std::span<const std::uint8_t> source);

[[nodiscard]] std::expected<std::vector<std::uint8_t>, CompressionError> compress(std::span<const std::uint8_t> source,
                                                                                  int level = 6);

} // namespace rawDeflate

namespace zstd {

inline constexpr int kDefaultLevel = 3;

[[nodiscard]] std::expected<std::vector<std::uint8_t>, CompressionError> compress(std::span<const std::uint8_t> source,
                                                                                  int level = kDefaultLevel);

[[nodiscard]] std::expected<std::vector<std::uint8_t>, CompressionError>
decompress(std::span<const std::uint8_t> source);

/// Never holds the input or the output whole.
class StreamCompressor {
public:
    using Sink = std::function<void(std::span<const std::uint8_t>)>;

    explicit StreamCompressor(Sink sink, int level = kDefaultLevel);
    StreamCompressor(const StreamCompressor&) = delete;
    StreamCompressor& operator=(const StreamCompressor&) = delete;
    ~StreamCompressor();

    [[nodiscard]] bool valid() const noexcept;

    bool feed(std::span<const std::uint8_t> piece);

    bool finish();

private:
    struct State;

    std::unique_ptr<State> state_;
};

class StreamDecompressor {
public:
    explicit StreamDecompressor(std::span<const std::uint8_t> source);
    explicit StreamDecompressor(std::vector<std::uint8_t>&&) = delete;
    StreamDecompressor(const StreamDecompressor&) = delete;
    StreamDecompressor& operator=(const StreamDecompressor&) = delete;
    ~StreamDecompressor();

    [[nodiscard]] bool valid() const noexcept;

    bool readFully(std::span<std::uint8_t> destination);

    [[nodiscard]] bool frameComplete() const noexcept;

private:
    struct State;

    std::unique_ptr<State> state_;
};

} // namespace zstd

} // namespace basalt::wire::io
