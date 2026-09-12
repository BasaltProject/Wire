#pragma once

#include <basalt/wire/bedrock/RakNet.hpp>
#include <basalt/wire/io/ByteBuffer.hpp>

#include <cstdint>
#include <span>
#include <vector>

namespace basalt::wire::bedrock {

/// One message inside a datagram, with whatever counters its reliability needs. RakNet calls this
/// an encapsulated packet; a datagram carries as many as fit.
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

    /// What this frame costs inside a datagram, header included.
    [[nodiscard]] std::size_t encodedSize() const noexcept;

    void encode(io::ByteBuffer& buffer) const;

    /// False when the buffer ran out or the reliability was not one of the eight, which is as far as
    /// a decoder gets to disagree with a peer: the datagram is dropped whole.
    [[nodiscard]] static bool decode(io::ByteBuffer& buffer, RakFrame& into);
};

/// A run of datagram sequence numbers, which is how RakNet acknowledges: one entry covers a range,
/// and a lone number is a range of one.
struct SequenceRange {
    std::uint32_t start = 0;
    std::uint32_t end = 0;
};

void writeRanges(io::ByteBuffer& buffer, std::span<const SequenceRange> ranges);

[[nodiscard]] bool readRanges(io::ByteBuffer& buffer, std::vector<SequenceRange>& into);

/// Turns a sorted list of sequence numbers into the ranges an ACK or NAK carries.
void collapseRanges(std::span<const std::uint32_t> sorted, std::vector<SequenceRange>& into);

} // namespace basalt::wire::bedrock
