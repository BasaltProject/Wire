#include <basalt/wire/bedrock/BedrockBatch.hpp>
#include <basalt/wire/bedrock/BedrockTypes.hpp>
#include <basalt/wire/io/Compression.hpp>

namespace basalt::wire::bedrock {
namespace {

constexpr std::size_t kMaxBatchSize = 16U * 1024U * 1024U;

}

bool packBatch(std::span<const std::vector<std::uint8_t>> packets, const BatchSettings& settings,
               io::ByteBuffer& destination) {
    io::ByteBuffer body{512};
    for (const std::vector<std::uint8_t>& packet : packets) {
        if (packet.empty()) {
            return false;
        }
        writeVarUInt(body, static_cast<std::uint32_t>(packet.size()));
        body.writeBytes(packet);
    }

    if (!settings.enabled) {
        destination.writeBytes(body.readable());
        return true;
    }

    const bool worthIt = body.readableBytes() >= settings.threshold && settings.algorithm != Compression::None;
    if (!worthIt) {
        destination.writeByte(static_cast<std::uint8_t>(Compression::None));
        destination.writeBytes(body.readable());
        return true;
    }

    const auto compressed = io::rawDeflate::compress(body.readable(), settings.level);
    if (!compressed) {
        return false;
    }
    destination.writeByte(static_cast<std::uint8_t>(Compression::Zlib));
    destination.writeBytes(*compressed);
    return true;
}

bool unpackBatch(std::span<const std::uint8_t> payload, const BatchSettings& settings,
                 std::vector<std::vector<std::uint8_t>>& packets) {
    if (payload.empty()) {
        return false;
    }

    std::span<const std::uint8_t> body = payload;
    std::vector<std::uint8_t> inflated;

    if (settings.enabled) {
        const auto algorithm = static_cast<Compression>(payload[0]);
        body = payload.subspan(1);
        if (algorithm == Compression::Zlib) {
            auto result = io::rawDeflate::decompressUnknownSize(body);
            if (!result || result->size() > kMaxBatchSize) {
                return false;
            }
            inflated = std::move(*result);
            body = inflated;
        } else if (algorithm != Compression::None) {
            return false;
        }
    }

    io::ByteBuffer reader = io::ByteBuffer::wrap({body.begin(), body.end()});
    while (reader.readableBytes() > 0) {
        const std::uint32_t length = readVarUInt(reader);
        if (reader.failed() || length == 0 || length > reader.readableBytes()) {
            return false;
        }
        const std::span<const std::uint8_t> packet = reader.readSpan(length);
        packets.emplace_back(packet.begin(), packet.end());
    }
    return !reader.failed();
}

} // namespace basalt::wire::bedrock
