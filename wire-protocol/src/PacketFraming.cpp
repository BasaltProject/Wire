#include <basalt/wire/io/Compression.hpp>
#include <basalt/wire/protocol/PacketFraming.hpp>
#include <basalt/wire/protocol/VarInt.hpp>

#include <cstring>

namespace basalt::wire::protocol::packetFraming {
namespace {

struct PeekedVarInt {
    std::int32_t value = 0;
    std::size_t byteLength = 0;
};

/// Reads the frame-length varint without consuming it, so a partial frame can be left alone until
/// the rest of it arrives.
std::expected<std::optional<PeekedVarInt>, FramingError> peekVarInt(const io::ByteBuffer& buffer) {
    std::uint32_t result = 0;
    for (std::size_t offset = 0; offset < varInt::kMaxBytes; ++offset) {
        if (offset >= buffer.readableBytes()) {
            return std::optional<PeekedVarInt>{};
        }
        const std::uint8_t current = buffer.peekByte(offset);
        result |= static_cast<std::uint32_t>(current & 0x7FU) << (offset * 7);
        if ((current & 0x80U) == 0) {
            return std::optional<PeekedVarInt>{PeekedVarInt{static_cast<std::int32_t>(result), offset + 1}};
        }
    }
    return std::unexpected{FramingError::MalformedVarInt};
}

} // namespace

void writeUncompressed(io::ByteBuffer& output, std::span<const std::uint8_t> packetIdAndData) {
    varInt::write(output, static_cast<std::int32_t>(packetIdAndData.size()));
    output.writeBytes(packetIdAndData);
}

std::expected<void, FramingError> writeCompressed(io::ByteBuffer& output, std::span<const std::uint8_t> packetIdAndData,
                                                  std::int32_t threshold) {
    if (packetIdAndData.size() > static_cast<std::size_t>(kMaxUncompressedSize)) {
        return std::unexpected{FramingError::FrameTooBig};
    }

    const auto size = static_cast<std::int32_t>(packetIdAndData.size());
    if (size < threshold) {
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
        return std::unexpected{FramingError::DecompressionFailed};
    }

    const auto bodyLength = static_cast<std::int32_t>(output.writerIndex() - bodyAt);
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

std::expected<bool, FramingError> tryReadFrame(io::ByteBuffer& input, bool compressionEnabled, io::ByteBuffer& into) {
    const auto peeked = peekVarInt(input);
    if (!peeked) {
        return std::unexpected{peeked.error()};
    }
    if (!peeked->has_value()) {
        return false;
    }

    const std::int32_t frameLength = (*peeked)->value;
    if (frameLength < 0) {
        return std::unexpected{FramingError::NegativeLength};
    }
    if (frameLength > kMaxUncompressedSize) {
        return std::unexpected{FramingError::FrameTooBig};
    }
    if (input.readableBytes() < (*peeked)->byteLength + static_cast<std::size_t>(frameLength)) {
        return false;
    }

    input.skip((*peeked)->byteLength);
    into.clear();

    if (!compressionEnabled) {
        into.writeBytes(input.readSpan(static_cast<std::size_t>(frameLength)));
        return true;
    }

    const std::size_t frameStart = input.readerIndex();
    const std::int32_t dataLength = varInt::read(input);
    if (input.failed() || dataLength < 0 || dataLength > kMaxUncompressedSize) {
        return std::unexpected{FramingError::MalformedVarInt};
    }

    const std::size_t consumed = input.readerIndex() - frameStart;
    const std::span<const std::uint8_t> payload = input.readSpan(static_cast<std::size_t>(frameLength) - consumed);
    if (dataLength == 0) {
        into.writeBytes(payload);
        return true;
    }
    if (!io::zlib::decompressInto(payload, static_cast<std::size_t>(dataLength), into)) {
        return std::unexpected{FramingError::DecompressionFailed};
    }
    return true;
}

} // namespace basalt::wire::protocol::packetFraming
