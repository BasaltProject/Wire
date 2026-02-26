#include <basalt/wire/bedrock/RakFrame.hpp>

namespace basalt::wire::bedrock {
namespace {

/// The header cost of a reliability, which is what decides where a split has to fall.
[[nodiscard]] std::size_t counterSize(Reliability reliability) noexcept {
    std::size_t size = 0;
    if (isReliable(reliability)) {
        size += 3;
    }
    if (isSequenced(reliability)) {
        size += 3;
    }
    if (isOrdered(reliability) || isSequenced(reliability)) {
        size += 4;
    }
    return size;
}

} // namespace

std::size_t RakFrame::encodedSize() const noexcept {
    return 3 + counterSize(reliability) + (split ? 10U : 0U) + body.size();
}

void RakFrame::encode(io::ByteBuffer& buffer) const {
    std::uint8_t flags = static_cast<std::uint8_t>(static_cast<std::uint8_t>(reliability) << 5U);
    if (split) {
        flags |= kFlagSplit;
    }
    buffer.writeByte(flags);
    buffer.writeShort(static_cast<std::int16_t>(body.size() << 3U));

    if (isReliable(reliability)) {
        writeMediumLE(buffer, reliableIndex);
    }
    if (isSequenced(reliability)) {
        writeMediumLE(buffer, sequenceIndex);
    }
    if (isOrdered(reliability) || isSequenced(reliability)) {
        writeMediumLE(buffer, orderIndex);
        buffer.writeByte(orderChannel);
    }
    if (split) {
        buffer.writeInt(static_cast<std::int32_t>(partCount));
        buffer.writeShort(static_cast<std::int16_t>(partId));
        buffer.writeInt(static_cast<std::int32_t>(partIndex));
    }
    buffer.writeBytes(body);
}

bool RakFrame::decode(io::ByteBuffer& buffer, RakFrame& into) {
    const std::uint8_t flags = buffer.readByte();
    const std::uint8_t reliabilityId = static_cast<std::uint8_t>(flags >> 5U);
    if (reliabilityId > 7) {
        return false;
    }
    into.reliability = static_cast<Reliability>(reliabilityId);
    into.split = (flags & kFlagSplit) != 0;

    const std::size_t bits = static_cast<std::uint16_t>(buffer.readShort());
    const std::size_t length = (bits + 7) / 8;
    if (length == 0) {
        return false;
    }

    if (isReliable(into.reliability)) {
        into.reliableIndex = readMediumLE(buffer);
    }
    if (isSequenced(into.reliability)) {
        into.sequenceIndex = readMediumLE(buffer);
    }
    if (isOrdered(into.reliability) || isSequenced(into.reliability)) {
        into.orderIndex = readMediumLE(buffer);
        into.orderChannel = buffer.readByte();
    }
    if (into.split) {
        into.partCount = static_cast<std::uint32_t>(buffer.readInt());
        into.partId = static_cast<std::uint16_t>(buffer.readShort());
        into.partIndex = static_cast<std::uint32_t>(buffer.readInt());
    }

    if (buffer.failed() || buffer.readableBytes() < length) {
        return false;
    }
    const std::span<const std::uint8_t> body = buffer.readSpan(length);
    into.body.assign(body.begin(), body.end());
    return !buffer.failed();
}

void writeRanges(io::ByteBuffer& buffer, std::span<const SequenceRange> ranges) {
    buffer.writeShort(static_cast<std::int16_t>(ranges.size()));
    for (const SequenceRange& range : ranges) {
        const bool single = range.start == range.end;
        buffer.writeByte(single ? 1 : 0);
        writeMediumLE(buffer, range.start);
        if (!single) {
            writeMediumLE(buffer, range.end);
        }
    }
}

bool readRanges(io::ByteBuffer& buffer, std::vector<SequenceRange>& into) {
    const auto count = static_cast<std::uint16_t>(buffer.readShort());
    into.clear();
    into.reserve(count);
    for (std::uint16_t index = 0; index < count; ++index) {
        const bool single = buffer.readByte() == 1;
        SequenceRange range{};
        range.start = readMediumLE(buffer);
        range.end = single ? range.start : readMediumLE(buffer);
        if (buffer.failed()) {
            return false;
        }
        into.push_back(range);
    }
    return true;
}

void collapseRanges(std::span<const std::uint32_t> sorted, std::vector<SequenceRange>& into) {
    into.clear();
    for (const std::uint32_t sequence : sorted) {
        if (!into.empty() && into.back().end + 1 == sequence) {
            into.back().end = sequence;
            continue;
        }
        into.push_back(SequenceRange{sequence, sequence});
    }
}

} // namespace basalt::wire::bedrock
