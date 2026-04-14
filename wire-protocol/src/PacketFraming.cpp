#include <basalt/wire/io/Compression.hpp>
#include <basalt/wire/protocol/PacketFraming.hpp>
#include <basalt/wire/protocol/VarInt.hpp>

#include <cstdint>
#include <cstring>

namespace basalt::wire::protocol::packetFraming {
namespace {

struct PeekedVarInt {
    std::int32_t value = 0;
    std::size_t byteLength = 0;
};

/// Reads the frame-length varint without consuming it, so a partial frame can be left alone until
/// the rest of it arrives.
std::expected<std::optional<PeekedVarInt>, FramingError> peekVarInt(std::span<const std::uint8_t> bytes,
                                                                    std::size_t maxBytes) {
    std::uint32_t result = 0;
    for (std::size_t offset = 0; offset < maxBytes; ++offset) {
        if (offset >= bytes.size()) {
            return std::optional<PeekedVarInt>{};
        }
        const std::uint8_t current = bytes[offset];
        if (offset == 4 && (current & 0xF0U) != 0) {
            return std::unexpected{FramingError::MalformedVarInt};
        }
        result |= static_cast<std::uint32_t>(current & 0x7FU) << (offset * 7);
        if ((current & 0x80U) == 0) {
            return std::optional<PeekedVarInt>{PeekedVarInt{static_cast<std::int32_t>(result), offset + 1}};
        }
    }
    return std::unexpected{maxBytes == 3 ? FramingError::FrameTooBig : FramingError::MalformedVarInt};
}

} // namespace

std::expected<void, FramingError> writeUncompressed(io::ByteBuffer& output,
                                                    std::span<const std::uint8_t> packetIdAndData) {
    if (packetIdAndData.empty()) {
        return std::unexpected{FramingError::EmptyFrame};
    }
    if (packetIdAndData.size() > static_cast<std::size_t>(kMaxFrameSize)) {
        return std::unexpected{FramingError::FrameTooBig};
    }
    varInt::write(output, static_cast<std::int32_t>(packetIdAndData.size()));
    output.writeBytes(packetIdAndData);
    return {};
}

std::expected<void, FramingError> writeCompressed(io::ByteBuffer& output, std::span<const std::uint8_t> packetIdAndData,
                                                  std::int32_t threshold) {
    if (packetIdAndData.empty()) {
        return std::unexpected{FramingError::EmptyFrame};
    }
    if (packetIdAndData.size() > static_cast<std::size_t>(kMaxUncompressedSize)) {
        return std::unexpected{FramingError::FrameTooBig};
    }

    const auto size = static_cast<std::int32_t>(packetIdAndData.size());
    if (size < threshold) {
        if (varInt::sizeOf(0) + size > kMaxFrameSize) {
            return std::unexpected{FramingError::FrameTooBig};
        }
        varInt::write(output, varInt::sizeOf(0) + size);
        varInt::write(output, 0);
        output.writeBytes(packetIdAndData);
        return {};
    }

    // The frame length is not known until the body is compressed, and the body goes straight into
    // the output buffer. Reserve the widest varint, write the body after it, then fill the reserved
    // bytes in: no staging buffer, no second copy of the packet.
    const std::size_t lengthAt = output.writerIndex();
    for (int i = 0; i < varInt::kMaxBytes; ++i) {
        output.writeByte(0);
    }
    const std::size_t bodyAt = output.writerIndex();
    varInt::write(output, size);
    const auto compressed = io::zlib::compressInto(packetIdAndData, output);
    if (!compressed) {
        output.trimTail(output.writerIndex() - lengthAt);
        return std::unexpected{FramingError::DecompressionFailed};
    }

    const auto bodyLength = static_cast<std::int32_t>(output.writerIndex() - bodyAt);
    if (bodyLength > kMaxFrameSize) {
        output.trimTail(output.writerIndex() - lengthAt);
        return std::unexpected{FramingError::FrameTooBig};
    }
    io::ByteBuffer header{varInt::kMaxBytes};
    varInt::write(header, bodyLength);
    const std::span<const std::uint8_t> headerBytes = header.readable();

    // The reserved run is five bytes wide and the real length may be shorter, so the body slides
    // back over the unused slack.
    std::span<std::uint8_t> frame = output.mutableSpan(lengthAt);
    std::memmove(frame.data() + headerBytes.size(), frame.data() + varInt::kMaxBytes, output.writerIndex() - bodyAt);
    std::memcpy(frame.data(), headerBytes.data(), headerBytes.size());
    output.trimTail(varInt::kMaxBytes - headerBytes.size());
    return {};
}

std::expected<bool, FramingError> tryReadFrame(io::ByteBuffer& input, bool compressionEnabled, io::ByteBuffer& into,
                                               std::optional<std::int32_t> threshold) {
    // parity: 26.3 Varint21FrameDecoder. Packet lengths have only three bytes, unlike packet IDs.
    const auto peeked = peekVarInt(input.readable(), 3);
    if (!peeked) {
        return std::unexpected{peeked.error()};
    }
    if (!peeked->has_value()) {
        return false;
    }
    const PeekedVarInt header = **peeked;
    if (header.value == 0) {
        return std::unexpected{FramingError::EmptyFrame};
    }
    const auto frameLength = static_cast<std::size_t>(header.value);
    if (input.readableBytes() - header.byteLength < frameLength) {
        return false;
    }

    // Limit every read to this frame. A truncated compression header must never borrow the next one.
    const auto body = input.readable().subspan(header.byteLength, frameLength);
    into.clear();
    if (!compressionEnabled) {
        into.writeBytes(body);
    } else {
        const auto dataHeader = peekVarInt(body, varInt::kMaxBytes);
        if (!dataHeader || !dataHeader->has_value()) {
            return std::unexpected{FramingError::MalformedVarInt};
        }
        const PeekedVarInt data = **dataHeader;
        if (data.value < 0) {
            return std::unexpected{FramingError::NegativeLength};
        }
        if (data.value > kMaxUncompressedSize) {
            return std::unexpected{FramingError::FrameTooBig};
        }
        const auto payload = body.subspan(data.byteLength);
        if (payload.empty()) {
            return std::unexpected{FramingError::EmptyFrame};
        }
        if (data.value == 0) {
            into.writeBytes(payload);
        } else {
            if (threshold && data.value < *threshold) {
                return std::unexpected{FramingError::BelowCompressionThreshold};
            }
            if (!io::zlib::decompressInto(payload, static_cast<std::size_t>(data.value), into) ||
                into.readableBytes() != static_cast<std::size_t>(data.value)) {
                into.clear();
                return std::unexpected{FramingError::DecompressionFailed};
            }
        }
    }
    input.skip(header.byteLength + frameLength);
    return true;
}

} // namespace basalt::wire::protocol::packetFraming
