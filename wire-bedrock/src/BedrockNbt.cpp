#include <basalt/wire/bedrock/BedrockNbt.hpp>
#include <basalt/wire/bedrock/BedrockTypes.hpp>

namespace basalt::wire::bedrock::nbt {

namespace {

/// The tag numbers, which both editions share even though nothing else about the format is shared.
constexpr std::uint8_t kEnd = 0x00;
constexpr std::uint8_t kByte = 0x01;
constexpr std::uint8_t kInt = 0x03;
constexpr std::uint8_t kFloat = 0x05;
constexpr std::uint8_t kString = 0x08;
constexpr std::uint8_t kCompound = 0x0a;

void writeName(io::ByteBuffer& buffer, const std::uint8_t tag, const std::string_view name) {
    buffer.writeByte(tag);
    writeVarUInt(buffer, static_cast<std::uint32_t>(name.size()));
    buffer.writeBytes(std::span{reinterpret_cast<const std::uint8_t*>(name.data()), name.size()});
}

} // namespace

void beginCompound(io::ByteBuffer& buffer, const std::string_view name) { writeName(buffer, kCompound, name); }

void beginNamed(io::ByteBuffer& buffer, const std::string_view name) { writeName(buffer, kCompound, name); }

void end(io::ByteBuffer& buffer) { buffer.writeByte(kEnd); }

void writeByte(io::ByteBuffer& buffer, const std::string_view name, const std::int8_t value) {
    writeName(buffer, kByte, name);
    buffer.writeByte(static_cast<std::uint8_t>(value));
}

void writeInt(io::ByteBuffer& buffer, const std::string_view name, const std::int32_t value) {
    writeName(buffer, kInt, name);
    writeVarInt(buffer, value);
}

void writeFloat(io::ByteBuffer& buffer, const std::string_view name, const float value) {
    writeName(buffer, kFloat, name);
    writeFloatLE(buffer, value);
}

void writeString(io::ByteBuffer& buffer, const std::string_view name, const std::string_view value) {
    writeName(buffer, kString, name);
    writeVarUInt(buffer, static_cast<std::uint32_t>(value.size()));
    buffer.writeBytes(std::span{reinterpret_cast<const std::uint8_t*>(value.data()), value.size()});
}

} // namespace basalt::wire::bedrock::nbt
