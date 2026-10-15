#pragma once

#include <basalt/wire/io/ByteBuffer.hpp>

#include <cstdint>
#include <span>
#include <vector>

namespace basalt::wire::bedrock {

enum class Compression : std::uint8_t {
    Zlib = 0x00,
    Snappy = 0x01,
    None = 0xff,
};

/// What a session agreed with its client in the network settings, which is the first thing either side says.
struct BatchSettings {
    bool enabled = false;
    Compression algorithm = Compression::Zlib;

    /// Compressing eighty bytes costs more than it saves and the client knows to expect either.
    std::uint16_t threshold = 256;

    int level = 6;
};

[[nodiscard]] bool packBatch(std::span<const std::vector<std::uint8_t>> packets, const BatchSettings& settings,
                             io::ByteBuffer& destination);

/// False when it was not a batch: a bad compression byte, a length past the end, or a stream that did not decompress.
[[nodiscard]] bool unpackBatch(std::span<const std::uint8_t> payload, const BatchSettings& settings,
                               std::vector<std::vector<std::uint8_t>>& packets);

} // namespace basalt::wire::bedrock
