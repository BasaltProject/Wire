#include <basalt/wire/bedrock/BedrockTypes.hpp>

#include <bit>

namespace basalt::wire::bedrock {
namespace {

constexpr int kMaxVarUIntBytes = 5;
constexpr int kMaxVarULongBytes = 10;

} // namespace

void writeVarUInt(io::ByteBuffer& buffer, std::uint32_t value) {
    while (value >= 0x80U) {
        buffer.writeByte(static_cast<std::uint8_t>(value | 0x80U));
        value >>= 7U;
    }
    buffer.writeByte(static_cast<std::uint8_t>(value));
}

std::uint32_t readVarUInt(io::ByteBuffer& buffer) noexcept {
    std::uint32_t result = 0;
    for (int index = 0; index < kMaxVarUIntBytes; ++index) {
        const std::uint8_t part = buffer.readByte();
        result |= static_cast<std::uint32_t>(part & 0x7fU) << (7 * index);
        if ((part & 0x80U) == 0) {
            return result;
        }
    }
    buffer.markFailed();
    return 0;
}

void writeVarInt(io::ByteBuffer& buffer, std::int32_t value) {
    const auto zigzag = static_cast<std::uint32_t>((value << 1) ^ (value >> 31));
    writeVarUInt(buffer, zigzag);
}

std::int32_t readVarInt(io::ByteBuffer& buffer) noexcept {
    const std::uint32_t zigzag = readVarUInt(buffer);
    return static_cast<std::int32_t>((zigzag >> 1U) ^ (~(zigzag & 1U) + 1U));
}

void writeVarULong(io::ByteBuffer& buffer, std::uint64_t value) {
    while (value >= 0x80U) {
        buffer.writeByte(static_cast<std::uint8_t>(value | 0x80U));
        value >>= 7U;
    }
    buffer.writeByte(static_cast<std::uint8_t>(value));
}

std::uint64_t readVarULong(io::ByteBuffer& buffer) noexcept {
    std::uint64_t result = 0;
    for (int index = 0; index < kMaxVarULongBytes; ++index) {
        const std::uint8_t part = buffer.readByte();
        result |= static_cast<std::uint64_t>(part & 0x7fU) << (7 * index);
        if ((part & 0x80U) == 0) {
            return result;
        }
    }
    buffer.markFailed();
    return 0;
}

void writeVarLong(io::ByteBuffer& buffer, std::int64_t value) {
    const auto zigzag = static_cast<std::uint64_t>((value << 1) ^ (value >> 63));
    writeVarULong(buffer, zigzag);
}

std::int64_t readVarLong(io::ByteBuffer& buffer) noexcept {
    const std::uint64_t zigzag = readVarULong(buffer);
    return static_cast<std::int64_t>((zigzag >> 1U) ^ (~(zigzag & 1U) + 1U));
}

void writeShortLE(io::ByteBuffer& buffer, std::uint16_t value) {
    buffer.writeByte(static_cast<std::uint8_t>(value & 0xffU));
    buffer.writeByte(static_cast<std::uint8_t>(value >> 8U));
}

std::uint16_t readShortLE(io::ByteBuffer& buffer) noexcept {
    const std::uint16_t low = buffer.readByte();
    const std::uint16_t high = buffer.readByte();
    return static_cast<std::uint16_t>(low | static_cast<std::uint16_t>(high << 8U));
}

void writeIntLE(io::ByteBuffer& buffer, std::int32_t value) {
    const auto unsignedValue = static_cast<std::uint32_t>(value);
    for (int shift = 0; shift < 32; shift += 8) {
        buffer.writeByte(static_cast<std::uint8_t>((unsignedValue >> shift) & 0xffU));
    }
}

std::int32_t readIntLE(io::ByteBuffer& buffer) noexcept {
    std::uint32_t result = 0;
    for (int shift = 0; shift < 32; shift += 8) {
        result |= static_cast<std::uint32_t>(buffer.readByte()) << shift;
    }
    return static_cast<std::int32_t>(result);
}

void writeLongLE(io::ByteBuffer& buffer, std::int64_t value) {
    const auto unsignedValue = static_cast<std::uint64_t>(value);
    for (int shift = 0; shift < 64; shift += 8) {
        buffer.writeByte(static_cast<std::uint8_t>((unsignedValue >> shift) & 0xffU));
    }
}

std::int64_t readLongLE(io::ByteBuffer& buffer) noexcept {
    std::uint64_t result = 0;
    for (int shift = 0; shift < 64; shift += 8) {
        result |= static_cast<std::uint64_t>(buffer.readByte()) << shift;
    }
    return static_cast<std::int64_t>(result);
}

void writeFloatLE(io::ByteBuffer& buffer, float value) { writeIntLE(buffer, std::bit_cast<std::int32_t>(value)); }

float readFloatLE(io::ByteBuffer& buffer) noexcept { return std::bit_cast<float>(readIntLE(buffer)); }

void writeString(io::ByteBuffer& buffer, std::string_view value) {
    writeVarUInt(buffer, static_cast<std::uint32_t>(value.size()));
    buffer.writeBytes({reinterpret_cast<const std::uint8_t*>(value.data()), value.size()});
}

std::string readString(io::ByteBuffer& buffer, std::size_t maxLength) {
    const std::uint32_t length = readVarUInt(buffer);
    if (buffer.failed() || length > maxLength || length > buffer.readableBytes()) {
        buffer.markFailed();
        return {};
    }
    const std::span<const std::uint8_t> bytes = buffer.readSpan(length);
    return std::string{reinterpret_cast<const char*>(bytes.data()), bytes.size()};
}

void writeUuid(io::ByteBuffer& buffer, std::uint64_t high, std::uint64_t low) {
    writeLongLE(buffer, static_cast<std::int64_t>(high));
    writeLongLE(buffer, static_cast<std::int64_t>(low));
}

} // namespace basalt::wire::bedrock
