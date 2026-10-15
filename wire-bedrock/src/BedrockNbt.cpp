#include <basalt/wire/bedrock/BedrockNbt.hpp>
#include <basalt/wire/bedrock/BedrockTypes.hpp>

#include <functional>
#include <string>

namespace basalt::wire::bedrock::nbt {

namespace {

constexpr std::uint8_t kEnd = 0x00;
constexpr std::uint8_t kByte = 0x01;
constexpr std::uint8_t kShort = 0x02;
constexpr std::uint8_t kInt = 0x03;
constexpr std::uint8_t kFloat = 0x05;
constexpr std::uint8_t kString = 0x08;
constexpr std::uint8_t kList = 0x09;
constexpr std::uint8_t kCompound = 0x0a;

void writeName(io::ByteBuffer& buffer, const std::uint8_t tag, const std::string_view name) {
    buffer.writeByte(tag);
    writeVarUInt(buffer, static_cast<std::uint32_t>(name.size()));
    buffer.writeBytes(std::span{reinterpret_cast<const std::uint8_t*>(name.data()), name.size()});
}

[[nodiscard]] std::string readName(io::ByteBuffer& buffer) { return readString(buffer); }

[[nodiscard]] bool skipValue(io::ByteBuffer& buffer, std::uint8_t tag);

[[nodiscard]] bool walkCompound(io::ByteBuffer& buffer,
                                const std::function<bool(std::string_view, std::uint8_t)>& visit) {
    while (!buffer.failed()) {
        const std::uint8_t tag = buffer.readByte();
        if (tag == kEnd) {
            return true;
        }
        const std::string name = readName(buffer);
        if (visit(name, tag)) {
            return true;
        }
        if (!skipValue(buffer, tag)) {
            return false;
        }
    }
    return false;
}

bool skipValue(io::ByteBuffer& buffer, const std::uint8_t tag) {
    constexpr std::size_t kLongBytes = 8;

    switch (tag) {
    case kByte:
        static_cast<void>(buffer.readByte());
        return true;
    case kShort:
        static_cast<void>(readShortLE(buffer));
        return true;
    case kInt:
        static_cast<void>(readVarInt(buffer));
        return true;
    case kFloat:
        static_cast<void>(readFloatLE(buffer));
        return true;
    case kString:
        static_cast<void>(readString(buffer));
        return true;
    case kCompound:
        return walkCompound(buffer, [](std::string_view, std::uint8_t) { return false; });
    case kList: {
        const std::uint8_t element = buffer.readByte();
        const std::int32_t count = readVarInt(buffer);
        for (std::int32_t at = 0; at < count && !buffer.failed(); ++at) {
            if (!skipValue(buffer, element)) {
                return false;
            }
        }
        return !buffer.failed();
    }
    case 0x04:
        for (std::size_t at = 0; at < kLongBytes; ++at) {
            static_cast<void>(buffer.readByte());
        }
        return true;
    default:
        return false;
    }
}

} // namespace

void beginCompound(io::ByteBuffer& buffer, const std::string_view name) { writeName(buffer, kCompound, name); }

void beginNamed(io::ByteBuffer& buffer, const std::string_view name) { writeName(buffer, kCompound, name); }

void beginCompoundList(io::ByteBuffer& buffer, const std::string_view name, const std::int32_t count) {
    writeName(buffer, kList, name);
    buffer.writeByte(kCompound);
    writeVarInt(buffer, count);
}

void writeStringList(io::ByteBuffer& buffer, const std::string_view name,
                     const std::span<const std::string_view> values) {
    writeName(buffer, kList, name);
    buffer.writeByte(kString);
    writeVarInt(buffer, static_cast<std::int32_t>(values.size()));
    for (const std::string_view value : values) {
        writeVarUInt(buffer, static_cast<std::uint32_t>(value.size()));
        buffer.writeBytes(std::span{reinterpret_cast<const std::uint8_t*>(value.data()), value.size()});
    }
}

void end(io::ByteBuffer& buffer) { buffer.writeByte(kEnd); }

void writeShort(io::ByteBuffer& buffer, const std::string_view name, const std::int16_t value) {
    writeName(buffer, kShort, name);
    writeShortLE(buffer, static_cast<std::uint16_t>(value));
}

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

std::optional<std::string> readStringAt(io::ByteBuffer& buffer, const std::span<const std::string_view> path) {
    if (path.empty() || buffer.readByte() != kCompound) {
        return std::nullopt;
    }
    static_cast<void>(readName(buffer));
    std::optional<std::string> found;
    std::size_t at = 0;
    while (at < path.size() && !buffer.failed()) {
        const bool last = at + 1 == path.size();
        bool entered = false;
        const bool walked = walkCompound(buffer, [&](const std::string_view name, const std::uint8_t tag) {
            if (name != path[at]) {
                return false;
            }
            if (last && tag == kString) {
                found = readString(buffer);
                entered = true;
                return true;
            }
            if (!last && tag == kCompound) {
                entered = true;
                return true;
            }
            return false;
        });
        if (!walked || !entered) {
            return std::nullopt;
        }
        ++at;
    }
    return found;
}

} // namespace basalt::wire::bedrock::nbt
