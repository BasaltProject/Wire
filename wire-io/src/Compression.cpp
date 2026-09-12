#include <basalt/wire/io/Compression.hpp>

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <zlib.h>

namespace basalt::wire::io {
namespace {

constexpr int kGzipWindowBits = 31;
constexpr int kRawDeflateWindowBits = -15;
constexpr int kDefaultMemLevel = 8;

CompressionError translate(int code) noexcept {
    switch (code) {
    case Z_BUF_ERROR:
        return CompressionError::BufferTooSmall;
    case Z_MEM_ERROR:
        return CompressionError::OutOfMemory;
    default:
        return CompressionError::CorruptData;
    }
}

/// Runs one whole stream through zlib with the given window bits, growing the output as needed.
std::expected<std::vector<std::uint8_t>, CompressionError>
inflateWholeStream(std::span<const std::uint8_t> source, int windowBits, std::size_t initialCapacity) {
    if (source.empty()) {
        return std::vector<std::uint8_t>{};
    }

    z_stream stream{};
    const int initResult = inflateInit2(&stream, windowBits);
    if (initResult != Z_OK) {
        return std::unexpected{translate(initResult)};
    }

    std::vector<std::uint8_t> out(std::max<std::size_t>(initialCapacity, 1024));
    stream.next_in = const_cast<Bytef*>(source.data());
    stream.avail_in = static_cast<uInt>(source.size());
    std::size_t written = 0;

    while (true) {
        stream.next_out = out.data() + written;
        stream.avail_out = static_cast<uInt>(out.size() - written);
        const int result = inflate(&stream, Z_NO_FLUSH);
        written = out.size() - stream.avail_out;

        if (result == Z_STREAM_END) {
            inflateEnd(&stream);
            out.resize(written);
            return out;
        }
        if (result != Z_OK && result != Z_BUF_ERROR) {
            inflateEnd(&stream);
            return std::unexpected{translate(result)};
        }
        if (written < out.size()) {
            // zlib stopped without filling the buffer and without finishing: the input is truncated.
            inflateEnd(&stream);
            return std::unexpected{CompressionError::CorruptData};
        }
        if (out.size() >= kMaxDecompressedSize) {
            inflateEnd(&stream);
            return std::unexpected{CompressionError::TooLarge};
        }
        out.resize(std::min(out.size() * 2, kMaxDecompressedSize));
    }
}

} // namespace

namespace zlib {

std::expected<void, CompressionError> compressInto(std::span<const std::uint8_t> source, ByteBuffer& destination,
                                                   int level) {
    if (source.empty()) {
        return {};
    }

    const uLong bound = compressBound(static_cast<uLong>(source.size()));
    const std::span<std::uint8_t> tail = destination.writableTail(bound);
    uLongf written = bound;
    const int result = compress2(tail.data(), &written, source.data(), static_cast<uLong>(source.size()), level);
    if (result != Z_OK) {
        destination.trimTail(bound);
        return std::unexpected{translate(result)};
    }
    destination.trimTail(bound - written);
    return {};
}

std::expected<void, CompressionError> decompressInto(std::span<const std::uint8_t> source, std::size_t uncompressedSize,
                                                     ByteBuffer& destination) {
    if (uncompressedSize == 0) {
        return {};
    }
    if (uncompressedSize > kMaxDecompressedSize) {
        return std::unexpected{CompressionError::TooLarge};
    }
    if (source.empty()) {
        return std::unexpected{CompressionError::CorruptData};
    }

    const std::span<std::uint8_t> tail = destination.writableTail(uncompressedSize);
    uLongf written = uncompressedSize;
    const int result = uncompress(tail.data(), &written, source.data(), static_cast<uLong>(source.size()));
    if (result != Z_OK) {
        destination.trimTail(uncompressedSize);
        return std::unexpected{translate(result)};
    }
    destination.trimTail(uncompressedSize - written);
    return {};
}

std::expected<std::vector<std::uint8_t>, CompressionError> compress(std::span<const std::uint8_t> source, int level) {
    ByteBuffer buffer{source.size()};
    const auto result = compressInto(source, buffer, level);
    if (!result) {
        return std::unexpected{result.error()};
    }
    return buffer.toVector();
}

std::expected<std::vector<std::uint8_t>, CompressionError> decompress(std::span<const std::uint8_t> source,
                                                                      std::size_t uncompressedSize) {
    ByteBuffer buffer{uncompressedSize};
    const auto result = decompressInto(source, uncompressedSize, buffer);
    if (!result) {
        return std::unexpected{result.error()};
    }
    return buffer.toVector();
}

std::expected<std::vector<std::uint8_t>, CompressionError> decompressUnknownSize(std::span<const std::uint8_t> source) {
    return inflateWholeStream(source, MAX_WBITS, source.size() * 4 + 64);
}

} // namespace zlib

namespace gzip {

std::expected<std::vector<std::uint8_t>, CompressionError> compress(std::span<const std::uint8_t> source, int level) {
    z_stream stream{};
    const int initResult =
        deflateInit2(&stream, level, Z_DEFLATED, kGzipWindowBits, kDefaultMemLevel, Z_DEFAULT_STRATEGY);
    if (initResult != Z_OK) {
        return std::unexpected{translate(initResult)};
    }

    std::vector<std::uint8_t> out(deflateBound(&stream, static_cast<uLong>(source.size())) + 32);
    stream.next_in = const_cast<Bytef*>(source.data());
    stream.avail_in = static_cast<uInt>(source.size());
    stream.next_out = out.data();
    stream.avail_out = static_cast<uInt>(out.size());

    const int result = deflate(&stream, Z_FINISH);
    const std::size_t written = out.size() - stream.avail_out;
    deflateEnd(&stream);
    if (result != Z_STREAM_END) {
        return std::unexpected{translate(result)};
    }
    out.resize(written);
    return out;
}

std::expected<std::vector<std::uint8_t>, CompressionError> decompress(std::span<const std::uint8_t> source) {
    // RFC 1952 stores the uncompressed size in the last four bytes, little-endian and mod 2^32, so
    // the usual first guess is exact rather than a guess.
    std::size_t hint = source.size() * 4 + 64;
    if (source.size() >= 4) {
        std::uint32_t declared = 0;
        for (std::size_t i = 0; i < 4; ++i) {
            declared |= static_cast<std::uint32_t>(source[source.size() - 4 + i]) << (i * 8);
        }
        if (declared > 0 && declared <= kMaxDecompressedSize) {
            hint = declared;
        }
    }
    return inflateWholeStream(source, kGzipWindowBits, hint);
}

} // namespace gzip

namespace rawDeflate {

std::expected<std::vector<std::uint8_t>, CompressionError> decompress(std::span<const std::uint8_t> source,
                                                                      std::size_t uncompressedSize) {
    if (uncompressedSize == 0) {
        return std::vector<std::uint8_t>{};
    }
    if (uncompressedSize > kMaxDecompressedSize) {
        return std::unexpected{CompressionError::TooLarge};
    }
    return inflateWholeStream(source, kRawDeflateWindowBits, uncompressedSize);
}

std::expected<std::vector<std::uint8_t>, CompressionError> decompressUnknownSize(std::span<const std::uint8_t> source) {
    return inflateWholeStream(source, kRawDeflateWindowBits, source.size() * 4);
}

std::expected<std::vector<std::uint8_t>, CompressionError> compress(std::span<const std::uint8_t> source, int level) {
    if (source.empty()) {
        return std::vector<std::uint8_t>{};
    }

    z_stream stream{};
    const int initResult =
        deflateInit2(&stream, level, Z_DEFLATED, kRawDeflateWindowBits, kDefaultMemLevel, Z_DEFAULT_STRATEGY);
    if (initResult != Z_OK) {
        return std::unexpected{translate(initResult)};
    }

    std::vector<std::uint8_t> out(deflateBound(&stream, static_cast<uLong>(source.size())));
    stream.next_in = const_cast<Bytef*>(source.data());
    stream.avail_in = static_cast<uInt>(source.size());
    stream.next_out = out.data();
    stream.avail_out = static_cast<uInt>(out.size());

    const int result = deflate(&stream, Z_FINISH);
    const std::size_t written = out.size() - stream.avail_out;
    deflateEnd(&stream);
    if (result != Z_STREAM_END) {
        return std::unexpected{translate(result)};
    }
    out.resize(written);
    return out;
}

} // namespace rawDeflate

} // namespace basalt::wire::io
