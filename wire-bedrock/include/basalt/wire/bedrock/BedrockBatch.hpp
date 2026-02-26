#pragma once

#include <basalt/wire/io/ByteBuffer.hpp>

#include <cstdint>
#include <span>
#include <vector>

namespace basalt::wire::bedrock {

/// How a batch is compressed. The number is the byte that leads the batch once compression is on.
enum class Compression : std::uint8_t {
    Zlib = 0x00,
    Snappy = 0x01,
    None = 0xff,
};

/// What a session agreed with its client in the network settings, which is the first thing either
/// side says. Before that is settled every batch travels raw and without the leading byte.
struct BatchSettings {
    bool enabled = false;
    Compression algorithm = Compression::Zlib;

    /// Under this many bytes a batch goes out uncompressed, marked as such. Compressing eighty
    /// bytes costs more than it saves and the client knows to expect either.
    std::uint16_t threshold = 256;

    int level = 6;
};

/// Packs encoded packets into the payload one game message carries: a length-prefixed run of
/// packets, compressed whole.
///
/// False when a packet was too big to describe or the compressor refused, and the destination is
/// left as it was found.
[[nodiscard]] bool packBatch(std::span<const std::vector<std::uint8_t>> packets, const BatchSettings& settings,
                             io::ByteBuffer& destination);

/// Takes the payload apart into the packets it carried. False when it was not a batch: a bad
/// compression byte, a length past the end, or a stream that did not decompress.
[[nodiscard]] bool unpackBatch(std::span<const std::uint8_t> payload, const BatchSettings& settings,
                               std::vector<std::vector<std::uint8_t>>& packets);

} // namespace basalt::wire::bedrock
