#pragma once

#include <basalt/wire/bedrock/RakNet.hpp>
#include <basalt/wire/io/ByteBuffer.hpp>

#include <cstdint>
#include <span>
#include <vector>

namespace basalt::wire::bedrock {

struct RakFrame {
    Reliability reliability = Reliability::Unreliable;
    std::uint32_t reliableIndex = 0;
    std::uint32_t sequenceIndex = 0;
    std::uint32_t orderIndex = 0;
    std::uint8_t orderChannel = 0;

    bool split = false;
    std::uint32_t partCount = 0;
    std::uint16_t partId = 0;
    std::uint32_t partIndex = 0;

    std::vector<std::uint8_t> body;

    [[nodiscard]] std::size_t encodedSize() const noexcept;

    void encode(io::ByteBuffer& buffer) const;

    [[nodiscard]] static bool decode(io::ByteBuffer& buffer, RakFrame& into);
};

struct SequenceRange {
    std::uint32_t start = 0;
    std::uint32_t end = 0;
};

void writeRanges(io::ByteBuffer& buffer, std::span<const SequenceRange> ranges);

[[nodiscard]] bool readRanges(io::ByteBuffer& buffer, std::vector<SequenceRange>& into);

void collapseRanges(std::span<const std::uint32_t> sorted, std::vector<SequenceRange>& into);

} // namespace basalt::wire::bedrock
