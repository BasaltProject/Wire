#include "BinaryIO.hpp"
#include "ModifiedUtf8.hpp"

#include <basalt/wire/nbt/NbtIO.hpp>

#include <cstdint>

namespace basalt::wire::nbt::nbtIO {
namespace {

void writeString(ByteWriter& writer, std::string_view value) {
    const std::size_t length = modifiedUtf8::encodedLength(value);
    // A 16-bit length cannot represent an over-long string; callers build these, so write what fits.
    writer.writeShort(static_cast<std::int16_t>(static_cast<std::uint16_t>(length)));
    modifiedUtf8::encode(value, writer);
}

std::expected<std::string, NbtError> readString(ByteReader& reader) {
    const auto length = reader.readShort();
    if (!length) {
        return std::unexpected{length.error()};
    }
    return modifiedUtf8::decode(reader, static_cast<std::uint16_t>(*length));
}

void writePayload(ByteWriter& writer, const NbtTag& tag);

void writeNamedTag(ByteWriter& writer, std::string_view name, const NbtTag& tag) {
    writer.writeByte(static_cast<std::uint8_t>(tag.type()));
    if (tag.type() != NbtType::End) {
        writeString(writer, name);
        writePayload(writer, tag);
    }
}

void writePayload(ByteWriter& writer, const NbtTag& tag) {
    switch (tag.type()) {
    case NbtType::End:
        break;
    case NbtType::Byte:
        writer.writeByte(static_cast<std::uint8_t>(*tag.get<std::int8_t>()));
        break;
    case NbtType::Short:
        writer.writeShort(*tag.get<std::int16_t>());
        break;
    case NbtType::Int:
        writer.writeInt(*tag.get<std::int32_t>());
        break;
    case NbtType::Long:
        writer.writeLong(*tag.get<std::int64_t>());
        break;
    case NbtType::Float:
        writer.writeFloat(*tag.get<float>());
        break;
    case NbtType::Double:
        writer.writeDouble(*tag.get<double>());
        break;
    case NbtType::ByteArray: {
        const auto& value = *tag.get<std::vector<std::int8_t>>();
        writer.writeInt(static_cast<std::int32_t>(value.size()));
        for (const std::int8_t b : value) {
            writer.writeByte(static_cast<std::uint8_t>(b));
        }
        break;
    }
    case NbtType::String:
        writeString(writer, *tag.get<std::string>());
        break;
    case NbtType::List: {
        const auto& list = *tag.get<NbtList>();
        writer.writeByte(static_cast<std::uint8_t>(list.elementType()));
        writer.writeInt(static_cast<std::int32_t>(list.size()));
        for (const NbtTag& element : list) {
            writePayload(writer, element);
        }
        break;
    }
    case NbtType::Compound: {
        const auto& compound = *tag.get<NbtCompound>();
        for (std::size_t i = 0; i < compound.size(); ++i) {
            writeNamedTag(writer, compound.names()[i], compound.values()[i]);
        }
        writer.writeByte(static_cast<std::uint8_t>(NbtType::End));
        break;
    }
    case NbtType::IntArray: {
        const auto& value = *tag.get<std::vector<std::int32_t>>();
        writer.writeInt(static_cast<std::int32_t>(value.size()));
        for (const std::int32_t v : value) {
            writer.writeInt(v);
        }
        break;
    }
    case NbtType::LongArray: {
        const auto& value = *tag.get<std::vector<std::int64_t>>();
        writer.writeInt(static_cast<std::int32_t>(value.size()));
        for (const std::int64_t v : value) {
            writer.writeLong(v);
        }
        break;
    }
    }
}

std::expected<std::size_t, NbtError> readSize(ByteReader& reader) {
    const auto size = reader.readInt();
    if (!size) {
        return std::unexpected{size.error()};
    }
    if (*size < 0) {
        return std::unexpected{NbtError::NegativeSize};
    }
    if (static_cast<std::size_t>(*size) > reader.remaining()) {
        return std::unexpected{NbtError::UnexpectedEnd};
    }
    return static_cast<std::size_t>(*size);
}

std::expected<NbtTag, NbtError> readPayload(ByteReader& reader, NbtType type, int depth);

std::expected<NbtTag, NbtError> readList(ByteReader& reader, int depth) {
    const auto elementId = reader.readByte();
    if (!elementId) {
        return std::unexpected{elementId.error()};
    }
    const std::optional<NbtType> elementType = nbtTypeFromId(*elementId);
    if (!elementType) {
        return std::unexpected{NbtError::UnknownTagId};
    }
    const auto size = readSize(reader);
    if (!size) {
        return std::unexpected{size.error()};
    }

    std::vector<NbtTag> elements;
    elements.reserve(*size);
    for (std::size_t i = 0; i < *size; ++i) {
        auto element = readPayload(reader, *elementType, depth + 1);
        if (!element) {
            return std::unexpected{element.error()};
        }
        elements.push_back(std::move(*element));
    }
    return NbtTag::ofList(NbtList{*elementType, std::move(elements)});
}

std::expected<NbtTag, NbtError> readCompound(ByteReader& reader, int depth) {
    NbtCompound compound;
    while (true) {
        const auto entryId = reader.readByte();
        if (!entryId) {
            return std::unexpected{entryId.error()};
        }
        const std::optional<NbtType> entryType = nbtTypeFromId(*entryId);
        if (!entryType) {
            return std::unexpected{NbtError::UnknownTagId};
        }
        if (*entryType == NbtType::End) {
            return NbtTag::ofCompound(std::move(compound));
        }
        auto key = readString(reader);
        if (!key) {
            return std::unexpected{key.error()};
        }
        auto value = readPayload(reader, *entryType, depth + 1);
        if (!value) {
            return std::unexpected{value.error()};
        }
        compound.set(*key, std::move(*value));
    }
}

std::expected<NbtTag, NbtError> readPayload(ByteReader& reader, NbtType type, int depth) {
    if (depth > kMaxDepth) {
        return std::unexpected{NbtError::DepthLimitExceeded};
    }

    switch (type) {
    case NbtType::End:
        return NbtTag{};
    case NbtType::Byte:
        return reader.readByte().transform([](std::uint8_t v) { return NbtTag::ofByte(static_cast<std::int8_t>(v)); });
    case NbtType::Short:
        return reader.readShort().transform([](std::int16_t v) { return NbtTag::ofShort(v); });
    case NbtType::Int:
        return reader.readInt().transform([](std::int32_t v) { return NbtTag::ofInt(v); });
    case NbtType::Long:
        return reader.readLong().transform([](std::int64_t v) { return NbtTag::ofLong(v); });
    case NbtType::Float:
        return reader.readFloat().transform([](float v) { return NbtTag::ofFloat(v); });
    case NbtType::Double:
        return reader.readDouble().transform([](double v) { return NbtTag::ofDouble(v); });
    case NbtType::ByteArray: {
        const auto size = readSize(reader);
        if (!size) {
            return std::unexpected{size.error()};
        }
        std::vector<std::int8_t> value(*size);
        for (std::size_t i = 0; i < *size; ++i) {
            const auto byte = reader.readByte();
            if (!byte) {
                return std::unexpected{byte.error()};
            }
            value[i] = static_cast<std::int8_t>(*byte);
        }
        return NbtTag::ofByteArray(std::move(value));
    }
    case NbtType::String:
        return readString(reader).transform([](std::string v) { return NbtTag::ofString(std::move(v)); });
    case NbtType::List:
        return readList(reader, depth);
    case NbtType::Compound:
        return readCompound(reader, depth);
    case NbtType::IntArray: {
        const auto size = readSize(reader);
        if (!size) {
            return std::unexpected{size.error()};
        }
        std::vector<std::int32_t> value(*size);
        for (std::size_t i = 0; i < *size; ++i) {
            const auto v = reader.readInt();
            if (!v) {
                return std::unexpected{v.error()};
            }
            value[i] = *v;
        }
        return NbtTag::ofIntArray(std::move(value));
    }
    case NbtType::LongArray: {
        const auto size = readSize(reader);
        if (!size) {
            return std::unexpected{size.error()};
        }
        std::vector<std::int64_t> value(*size);
        for (std::size_t i = 0; i < *size; ++i) {
            const auto v = reader.readLong();
            if (!v) {
                return std::unexpected{v.error()};
            }
            value[i] = *v;
        }
        return NbtTag::ofLongArray(std::move(value));
    }
    }
    return std::unexpected{NbtError::UnknownTagId};
}

} // namespace

std::vector<std::uint8_t> write(const NbtTag& root, std::string_view name) {
    ByteWriter writer;
    writeNamedTag(writer, name, root);
    return writer.take();
}

std::expected<NamedTag, NbtError> read(std::span<const std::uint8_t> bytes) {
    ByteReader reader{bytes};
    const auto id = reader.readByte();
    if (!id) {
        return std::unexpected{id.error()};
    }
    const std::optional<NbtType> type = nbtTypeFromId(*id);
    if (!type) {
        return std::unexpected{NbtError::UnknownTagId};
    }
    if (*type == NbtType::End) {
        return NamedTag{};
    }

    auto name = readString(reader);
    if (!name) {
        return std::unexpected{name.error()};
    }
    auto tag = readPayload(reader, *type, 0);
    if (!tag) {
        return std::unexpected{tag.error()};
    }
    return NamedTag{std::move(*name), std::move(*tag)};
}

std::vector<std::uint8_t> writeAnonymous(const NbtTag& root) {
    ByteWriter writer;
    writer.writeByte(static_cast<std::uint8_t>(root.type()));
    if (root.type() != NbtType::End) {
        writePayload(writer, root);
    }
    return writer.take();
}

std::expected<AnonymousTag, NbtError> readAnonymous(std::span<const std::uint8_t> bytes) {
    ByteReader reader{bytes};
    const auto id = reader.readByte();
    if (!id) {
        return std::unexpected{id.error()};
    }
    const std::optional<NbtType> type = nbtTypeFromId(*id);
    if (!type) {
        return std::unexpected{NbtError::UnknownTagId};
    }
    if (*type == NbtType::End) {
        return AnonymousTag{NbtTag{}, reader.position()};
    }

    auto tag = readPayload(reader, *type, 0);
    if (!tag) {
        return std::unexpected{tag.error()};
    }
    return AnonymousTag{std::move(*tag), reader.position()};
}

} // namespace basalt::wire::nbt::nbtIO
