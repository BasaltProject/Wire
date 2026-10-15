#include <basalt/wire/common/json/Json.hpp>
#include <basalt/wire/protocol/ProtocolBuffer.hpp>
#include <basalt/wire/protocol/login/LoginPackets.hpp>
#include <basalt/wire/text/ComponentJson.hpp>

#include <cstdint>

namespace basalt::wire::protocol::login {

void LoginStartPacket::write(io::ByteBuffer& buffer) const {
    writeString(buffer, name);
    writeUuid(buffer, profileId);
}

std::optional<LoginStartPacket> LoginStartPacket::read(io::ByteBuffer& buffer) {
    LoginStartPacket packet;
    packet.name = readString(buffer, kMaxNameLength);
    packet.profileId = readUuid(buffer);
    return buffer.failed() ? std::nullopt : std::optional{std::move(packet)};
}

void HelloPacket::write(io::ByteBuffer& buffer) const {
    writeString(buffer, serverId);
    writeByteArray(buffer, publicKey);
    writeByteArray(buffer, challenge);
    writeBoolean(buffer, shouldAuthenticate);
}

std::optional<HelloPacket> HelloPacket::read(io::ByteBuffer& buffer) {
    HelloPacket packet;
    packet.serverId = readString(buffer, kMaxServerIdLength);
    packet.publicKey = readByteArray(buffer);
    packet.challenge = readByteArray(buffer);
    packet.shouldAuthenticate = readBoolean(buffer);
    return buffer.failed() ? std::nullopt : std::optional{std::move(packet)};
}

void KeyPacket::write(io::ByteBuffer& buffer) const {
    writeByteArray(buffer, encryptedSecret);
    writeByteArray(buffer, encryptedChallenge);
}

std::optional<KeyPacket> KeyPacket::read(io::ByteBuffer& buffer) {
    KeyPacket packet;
    packet.encryptedSecret = readByteArray(buffer);
    packet.encryptedChallenge = readByteArray(buffer);
    return buffer.failed() ? std::nullopt : std::optional{std::move(packet)};
}

void LoginCompressionPacket::write(io::ByteBuffer& buffer) const { varInt::write(buffer, threshold); }

std::optional<LoginCompressionPacket> LoginCompressionPacket::read(io::ByteBuffer& buffer) {
    LoginCompressionPacket packet;
    packet.threshold = varInt::read(buffer);
    return buffer.failed() ? std::nullopt : std::optional{packet};
}

void LoginFinishedPacket::write(io::ByteBuffer& buffer) const {
    writeUuid(buffer, gameProfile.id);
    writeString(buffer, gameProfile.name);
    varInt::write(buffer, static_cast<std::int32_t>(gameProfile.properties.size()));
    for (const GameProfileProperty& property : gameProfile.properties) {
        writeString(buffer, property.name);
        writeString(buffer, property.value);
        writeBoolean(buffer, property.signature.has_value());
        if (property.signature) {
            writeString(buffer, *property.signature);
        }
    }
    writeUuid(buffer, sessionId);
}

std::optional<LoginFinishedPacket> LoginFinishedPacket::read(io::ByteBuffer& buffer) {
    LoginFinishedPacket packet;
    packet.gameProfile.id = readUuid(buffer);
    packet.gameProfile.name = readString(buffer, 16);

    const std::int32_t count = readCollectionSize(buffer, 3);
    packet.gameProfile.properties.reserve(static_cast<std::size_t>(count));
    for (std::int32_t i = 0; i < count && !buffer.failed(); ++i) {
        GameProfileProperty property;
        property.name = readString(buffer);
        property.value = readString(buffer);
        if (readBoolean(buffer)) {
            property.signature = readString(buffer);
        }
        packet.gameProfile.properties.push_back(std::move(property));
    }

    packet.sessionId = readUuid(buffer);
    return buffer.failed() ? std::nullopt : std::optional{std::move(packet)};
}

void LoginDisconnectPacket::write(io::ByteBuffer& buffer) const {
    writeString(buffer, common::writeJson(text::componentJson::serialize(reason)));
}

} // namespace basalt::wire::protocol::login
