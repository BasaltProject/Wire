#include <basalt/wire/protocol/ProtocolBuffer.hpp>
#include <basalt/wire/protocol/play/CommandPackets.hpp>

#include <cstdint>

namespace basalt::wire::protocol::play {
namespace {

constexpr std::uint8_t kMaskKind = 3;
constexpr std::uint8_t kFlagExecutable = 4;
constexpr std::uint8_t kFlagRedirect = 8;
constexpr std::uint8_t kFlagCustomSuggestions = 16;

constexpr std::uint8_t kBoundMin = 1;
constexpr std::uint8_t kBoundMax = 2;

/// The flag byte and then whichever bounds it claims. Shared by the three bounded number parsers,
/// which differ only in the width of what follows.
template <typename T, typename Read>
ArgumentParser::Bounds<T> readBounds(io::ByteBuffer& buffer, Read&& read) {
    const std::uint8_t flags = buffer.readByte();
    ArgumentParser::Bounds<T> bounds;
    if ((flags & kBoundMin) != 0) {
        bounds.min = read(buffer);
    }
    if ((flags & kBoundMax) != 0) {
        bounds.max = read(buffer);
    }
    return bounds;
}

template <typename T, typename Write>
void writeBounds(io::ByteBuffer& buffer, const ArgumentParser::Bounds<T>& bounds, Write&& write) {
    std::uint8_t flags = 0;
    if (bounds.min) {
        flags |= kBoundMin;
    }
    if (bounds.max) {
        flags |= kBoundMax;
    }
    buffer.writeByte(flags);
    if (bounds.min) {
        write(buffer, *bounds.min);
    }
    if (bounds.max) {
        write(buffer, *bounds.max);
    }
}

} // namespace

void ArgumentParser::write(io::ByteBuffer& buffer) const {
    varInt::write(buffer, registryId_);

    if (const auto* string = as<StringProperties>()) {
        varInt::write(buffer, static_cast<std::int32_t>(string->kind));
    } else if (const auto* integer = as<Bounds<std::int32_t>>()) {
        writeBounds(buffer, *integer, [](io::ByteBuffer& out, std::int32_t value) { out.writeInt(value); });
    } else if (const auto* real = as<Bounds<double>>()) {
        writeBounds(buffer, *real, [](io::ByteBuffer& out, double value) { out.writeDouble(value); });
    } else if (const auto* single = as<Bounds<float>>()) {
        writeBounds(buffer, *single, [](io::ByteBuffer& out, float value) { out.writeFloat(value); });
    } else if (const auto* entity = as<EntityProperties>()) {
        std::uint8_t flags = 0;
        if (entity->single) {
            flags |= 1;
        }
        if (entity->playersOnly) {
            flags |= 2;
        }
        buffer.writeByte(flags);
    } else if (const auto* holder = as<ScoreHolderProperties>()) {
        buffer.writeByte(holder->multiple ? 1 : 0);
    } else if (const auto* time = as<TimeProperties>()) {
        buffer.writeInt(time->min);
    } else if (const auto* registry = as<RegistryProperties>()) {
        writeString(buffer, registry->registry);
    }
    // A singleton parser writes nothing after its id.
}

std::optional<ArgumentParser> ArgumentParser::read(io::ByteBuffer& buffer) {
    using namespace argumentType;

    const std::int32_t id = varInt::read(buffer);
    if (buffer.failed() || id < 0 || id >= kCount) {
        buffer.markFailed();
        return std::nullopt;
    }

    switch (id) {
    case kBrigadierString: {
        const std::int32_t kind = varInt::read(buffer);
        if (kind < 0 || kind > 2) {
            buffer.markFailed();
            return std::nullopt;
        }
        return ArgumentParser{id, StringProperties{static_cast<StringKind>(kind)}};
    }
    case kBrigadierInteger:
        return ArgumentParser{id, readBounds<std::int32_t>(buffer, [](io::ByteBuffer& in) { return in.readInt(); })};
    case kBrigadierDouble:
        return ArgumentParser{id, readBounds<double>(buffer, [](io::ByteBuffer& in) { return in.readDouble(); })};
    case kBrigadierFloat:
        return ArgumentParser{id, readBounds<float>(buffer, [](io::ByteBuffer& in) { return in.readFloat(); })};
    case kEntity: {
        const std::uint8_t flags = buffer.readByte();
        return ArgumentParser{id, EntityProperties{(flags & 1) != 0, (flags & 2) != 0}};
    }
    case kScoreHolder:
        return ArgumentParser{id, ScoreHolderProperties{(buffer.readByte() & 1) != 0}};
    case kTime:
        return ArgumentParser{id, TimeProperties{buffer.readInt()}};
    case kResourceOrTag:
    case kResourceOrTagKey:
    case kResource:
    case kResourceKey:
    case kResourceSelector:
        return ArgumentParser{id, RegistryProperties{readString(buffer)}};
    default:
        return ArgumentParser{id};
    }
}

void CommandNodeData::write(io::ByteBuffer& buffer) const {
    std::uint8_t flags = static_cast<std::uint8_t>(kind);
    if (executable) {
        flags |= kFlagExecutable;
    }
    if (redirect) {
        flags |= kFlagRedirect;
    }
    if (suggestionId) {
        flags |= kFlagCustomSuggestions;
    }
    buffer.writeByte(flags);

    varInt::write(buffer, static_cast<std::int32_t>(children.size()));
    for (const std::int32_t child : children) {
        varInt::write(buffer, child);
    }
    if (redirect) {
        varInt::write(buffer, *redirect);
    }
    if (kind != Kind::Root) {
        writeString(buffer, name.value_or(""));
    }
    if (kind == Kind::Argument && parser) {
        parser->write(buffer);
        if (suggestionId) {
            writeString(buffer, *suggestionId);
        }
    }
}

std::optional<CommandNodeData> CommandNodeData::read(io::ByteBuffer& buffer) {
    const std::uint8_t flags = buffer.readByte();
    const std::uint8_t rawKind = flags & kMaskKind;
    if (buffer.failed() || rawKind > static_cast<std::uint8_t>(Kind::Argument)) {
        buffer.markFailed();
        return std::nullopt;
    }

    CommandNodeData node;
    node.kind = static_cast<Kind>(rawKind);
    node.executable = (flags & kFlagExecutable) != 0;

    const std::int32_t count = readCollectionSize(buffer);
    node.children.reserve(static_cast<std::size_t>(count));
    for (std::int32_t i = 0; i < count && !buffer.failed(); ++i) {
        node.children.push_back(varInt::read(buffer));
    }

    if ((flags & kFlagRedirect) != 0) {
        node.redirect = varInt::read(buffer);
    }
    if (node.kind != Kind::Root) {
        node.name = readString(buffer);
    }
    if (node.kind == Kind::Argument) {
        auto parser = ArgumentParser::read(buffer);
        if (!parser) {
            return std::nullopt;
        }
        node.parser = std::move(*parser);
        if ((flags & kFlagCustomSuggestions) != 0) {
            node.suggestionId = readString(buffer);
        }
    }
    return buffer.failed() ? std::nullopt : std::optional{std::move(node)};
}

void CommandsPacket::write(io::ByteBuffer& buffer) const {
    varInt::write(buffer, static_cast<std::int32_t>(nodes.size()));
    for (const CommandNodeData& node : nodes) {
        node.write(buffer);
    }
    varInt::write(buffer, rootIndex);
}

std::optional<CommandsPacket> CommandsPacket::read(io::ByteBuffer& buffer) {
    CommandsPacket packet;
    const std::int32_t count = readCollectionSize(buffer, 2);
    packet.nodes.reserve(static_cast<std::size_t>(count));
    for (std::int32_t i = 0; i < count && !buffer.failed(); ++i) {
        auto node = CommandNodeData::read(buffer);
        if (!node) {
            return std::nullopt;
        }
        packet.nodes.push_back(std::move(*node));
    }
    packet.rootIndex = varInt::read(buffer);
    return buffer.failed() ? std::nullopt : std::optional{std::move(packet)};
}

void ChatCommandPacket::write(io::ByteBuffer& buffer) const { writeString(buffer, command); }

std::optional<ChatCommandPacket> ChatCommandPacket::read(io::ByteBuffer& buffer) {
    ChatCommandPacket packet{readString(buffer)};
    return buffer.failed() ? std::nullopt : std::optional{std::move(packet)};
}

} // namespace basalt::wire::protocol::play
