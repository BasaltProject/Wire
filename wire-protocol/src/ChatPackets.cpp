#include <basalt/wire/protocol/ProtocolBuffer.hpp>
#include <basalt/wire/protocol/VarInt.hpp>
#include <basalt/wire/protocol/play/ChatPackets.hpp>
#include <basalt/wire/text/ComponentNbt.hpp>

#include <algorithm>
#include <bit>

namespace basalt::wire::protocol::play {
namespace {

void writeComponent(io::ByteBuffer& buffer, const text::Component& component) {
    writeAnonymousNbt(buffer, text::componentNbt::serialize(component));
}

text::Component readComponent(io::ByteBuffer& buffer) {
    return text::componentNbt::deserialize(readAnonymousNbt(buffer));
}

void writeOptionalComponent(io::ByteBuffer& buffer, const std::optional<text::Component>& component) {
    writeBoolean(buffer, component.has_value());
    if (component) {
        writeComponent(buffer, *component);
    }
}

std::optional<text::Component> readOptionalComponent(io::ByteBuffer& buffer) {
    if (!readBoolean(buffer)) {
        return std::nullopt;
    }
    return readComponent(buffer);
}

void writeSignature(io::ByteBuffer& buffer, const MessageSignature& signature) { buffer.writeBytes(signature); }

MessageSignature readSignature(io::ByteBuffer& buffer) {
    MessageSignature signature{};
    const std::vector<std::uint8_t> bytes = buffer.readBytes(kMessageSignatureBytes);
    if (bytes.size() == kMessageSignatureBytes) {
        std::ranges::copy(bytes, signature.begin());
    } else {
        buffer.markFailed();
    }
    return signature;
}

void writeOptionalSignature(io::ByteBuffer& buffer, const std::optional<MessageSignature>& signature) {
    writeBoolean(buffer, signature.has_value());
    if (signature) {
        writeSignature(buffer, *signature);
    }
}

std::optional<MessageSignature> readOptionalSignature(io::ByteBuffer& buffer) {
    if (!readBoolean(buffer)) {
        return std::nullopt;
    }
    return readSignature(buffer);
}

} // namespace

std::size_t LastSeenUpdate::bitLength() const noexcept {
    for (std::size_t i = kBytes; i > 0; --i) {
        const std::uint8_t byte = acknowledged[i - 1];
        if (byte != 0) {
            return (i - 1) * 8 + static_cast<std::size_t>(std::bit_width(byte));
        }
    }
    return 0;
}

void LastSeenUpdate::write(io::ByteBuffer& buffer) const {
    varInt::write(buffer, offset);
    buffer.writeBytes(acknowledged);
    buffer.writeByte(static_cast<std::uint8_t>(checksum));
}

LastSeenUpdate LastSeenUpdate::read(io::ByteBuffer& buffer) {
    LastSeenUpdate update;
    update.offset = varInt::read(buffer);
    const std::vector<std::uint8_t> bytes = buffer.readBytes(kBytes);
    if (bytes.size() == kBytes) {
        std::ranges::copy(bytes, update.acknowledged.begin());
    } else {
        buffer.markFailed();
    }
    update.checksum = static_cast<std::int8_t>(buffer.readByte());
    return update;
}

void RemoteChatSessionData::write(io::ByteBuffer& buffer) const {
    writeUuid(buffer, sessionId);
    buffer.writeLong(expiresAt);
    writeByteArray(buffer, publicKey);
    writeByteArray(buffer, keySignature);
}

RemoteChatSessionData RemoteChatSessionData::read(io::ByteBuffer& buffer) {
    RemoteChatSessionData data;
    data.sessionId = readUuid(buffer);
    data.expiresAt = buffer.readLong();
    data.publicKey = readByteArray(buffer, kMaxPublicKeyBytes);
    data.keySignature = readByteArray(buffer, kMaxKeySignatureBytes);
    return data;
}

void ChatAckPacket::write(io::ByteBuffer& buffer) const { varInt::write(buffer, offset); }

std::optional<ChatAckPacket> ChatAckPacket::read(io::ByteBuffer& buffer) {
    ChatAckPacket packet{varInt::read(buffer)};
    return buffer.failed() ? std::nullopt : std::optional{packet};
}

void ChatMessagePacket::write(io::ByteBuffer& buffer) const {
    writeString(buffer, message);
    buffer.writeLong(timestamp);
    buffer.writeLong(salt);
    writeOptionalSignature(buffer, signature);
    lastSeen.write(buffer);
}

std::optional<ChatMessagePacket> ChatMessagePacket::read(io::ByteBuffer& buffer) {
    ChatMessagePacket packet;
    packet.message = readString(buffer, kMaxLength);
    packet.timestamp = buffer.readLong();
    packet.salt = buffer.readLong();
    packet.signature = readOptionalSignature(buffer);
    packet.lastSeen = LastSeenUpdate::read(buffer);
    return buffer.failed() ? std::nullopt : std::optional{std::move(packet)};
}

void SignedChatCommandPacket::write(io::ByteBuffer& buffer) const {
    writeString(buffer, command);
    buffer.writeLong(timestamp);
    buffer.writeLong(salt);
    varInt::write(buffer, static_cast<std::int32_t>(argumentSignatures.size()));
    for (const ArgumentSignature& argument : argumentSignatures) {
        writeString(buffer, argument.name);
        writeSignature(buffer, argument.signature);
    }
    lastSeen.write(buffer);
}

std::optional<SignedChatCommandPacket> SignedChatCommandPacket::read(io::ByteBuffer& buffer) {
    SignedChatCommandPacket packet;
    packet.command = readString(buffer);
    packet.timestamp = buffer.readLong();
    packet.salt = buffer.readLong();
    const std::int32_t count = readCollectionSize(buffer, static_cast<std::int32_t>(kMessageSignatureBytes));
    if (count > kMaxArguments) {
        buffer.markFailed();
        return std::nullopt;
    }
    for (std::int32_t i = 0; i < count && !buffer.failed(); ++i) {
        ArgumentSignature argument;
        argument.name = readString(buffer, ArgumentSignature::kMaxNameLength);
        argument.signature = readSignature(buffer);
        packet.argumentSignatures.push_back(std::move(argument));
    }
    packet.lastSeen = LastSeenUpdate::read(buffer);
    return buffer.failed() ? std::nullopt : std::optional{std::move(packet)};
}

void ChatSessionUpdatePacket::write(io::ByteBuffer& buffer) const { session.write(buffer); }

std::optional<ChatSessionUpdatePacket> ChatSessionUpdatePacket::read(io::ByteBuffer& buffer) {
    ChatSessionUpdatePacket packet{RemoteChatSessionData::read(buffer)};
    return buffer.failed() ? std::nullopt : std::optional{std::move(packet)};
}

void ChatTypeBound::write(io::ByteBuffer& buffer) const {
    // parity: `ByteBufCodecs.holder`, a registry reference as its id plus one; zero is an inline type.
    varInt::write(buffer, chatType + 1);
    writeComponent(buffer, name);
    writeOptionalComponent(buffer, targetName);
}

std::optional<ChatTypeBound> ChatTypeBound::read(io::ByteBuffer& buffer) {
    ChatTypeBound bound;
    // An inline chat type is a decoration this side cannot hold, so it is refused.
    bound.chatType = varInt::read(buffer) - 1;
    if (bound.chatType < 0) {
        buffer.markFailed();
        return std::nullopt;
    }
    bound.name = readComponent(buffer);
    bound.targetName = readOptionalComponent(buffer);
    return buffer.failed() ? std::nullopt : std::optional{std::move(bound)};
}

void PlayerChatPacket::write(io::ByteBuffer& buffer) const {
    varInt::write(buffer, globalIndex);
    writeUuid(buffer, sender);
    varInt::write(buffer, index);
    writeOptionalSignature(buffer, signature);
    writeString(buffer, content);
    buffer.writeLong(timestamp);
    buffer.writeLong(salt);
    varInt::write(buffer, static_cast<std::int32_t>(lastSeen.size()));
    for (const PackedSignature& packed : lastSeen) {
        varInt::write(buffer, packed.full.has_value() ? 0 : packed.id + 1);
        if (packed.full) {
            writeSignature(buffer, *packed.full);
        }
    }
    writeOptionalComponent(buffer, unsignedContent);
    varInt::write(buffer, static_cast<std::int32_t>(filterMask.type));
    if (filterMask.type == FilterMask::Type::PartiallyFiltered) {
        writeByteArray(buffer, filterMask.mask);
    }
    chatType.write(buffer);
}

std::optional<PlayerChatPacket> PlayerChatPacket::read(io::ByteBuffer& buffer) {
    PlayerChatPacket packet;
    packet.globalIndex = varInt::read(buffer);
    packet.sender = readUuid(buffer);
    packet.index = varInt::read(buffer);
    packet.signature = readOptionalSignature(buffer);
    packet.content = readString(buffer, ChatMessagePacket::kMaxLength);
    packet.timestamp = buffer.readLong();
    packet.salt = buffer.readLong();
    const std::int32_t count = readCollectionSize(buffer);
    if (count > static_cast<std::int32_t>(LastSeenUpdate::kWindow)) {
        buffer.markFailed();
        return std::nullopt;
    }
    for (std::int32_t i = 0; i < count && !buffer.failed(); ++i) {
        PackedSignature packed;
        packed.id = varInt::read(buffer) - 1;
        if (packed.id == PackedSignature::kFullSignature) {
            packed.full = readSignature(buffer);
        }
        packet.lastSeen.push_back(packed);
    }
    packet.unsignedContent = readOptionalComponent(buffer);
    // parity: `ByIdMap.OutOfBoundsStrategy.ZERO`, an unknown type reads as pass-through.
    const std::int32_t type = varInt::read(buffer);
    packet.filterMask.type = type == 1   ? FilterMask::Type::FullyFiltered
                             : type == 2 ? FilterMask::Type::PartiallyFiltered
                                         : FilterMask::Type::PassThrough;
    if (packet.filterMask.type == FilterMask::Type::PartiallyFiltered) {
        packet.filterMask.mask = readByteArray(buffer);
    }
    const std::optional<ChatTypeBound> bound = ChatTypeBound::read(buffer);
    if (!bound.has_value()) {
        return std::nullopt;
    }
    packet.chatType = *bound;
    return buffer.failed() ? std::nullopt : std::optional{std::move(packet)};
}

void DisguisedChatPacket::write(io::ByteBuffer& buffer) const {
    writeComponent(buffer, message);
    chatType.write(buffer);
}

std::optional<DisguisedChatPacket> DisguisedChatPacket::read(io::ByteBuffer& buffer) {
    DisguisedChatPacket packet;
    packet.message = readComponent(buffer);
    const std::optional<ChatTypeBound> bound = ChatTypeBound::read(buffer);
    if (!bound.has_value()) {
        return std::nullopt;
    }
    packet.chatType = *bound;
    return buffer.failed() ? std::nullopt : std::optional{std::move(packet)};
}

} // namespace basalt::wire::protocol::play
