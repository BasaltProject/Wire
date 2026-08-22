#include <basalt/wire/multiversion/ProtocolVersions.hpp>
#include <basalt/wire/multiversion/java/JavaTranslators.hpp>
#include <basalt/wire/multiversion/java/Link775Up.hpp>
#include <basalt/wire/multiversion/java/LoginFields.hpp>
#include <basalt/wire/protocol/ProtocolBuffer.hpp>
#include <basalt/wire/protocol/VarInt.hpp>
#include <basalt/wire/protocol/play/PlayPackets.hpp>
#include <basalt/wire/protocol/play/UiPackets.hpp>
#include <basalt/wire/text/ComponentNbt.hpp>

namespace basalt::wire::multiversion::java {

namespace {

constexpr std::int32_t kFirstNonColor = 16;

void copyText(io::ByteBuffer& in, io::ByteBuffer& out) {
    protocol::writeAnonymousNbt(
        out, text::componentNbt::serialize(text::componentNbt::deserialize(protocol::readAnonymousNbt(in))));
}

/// parity: 26.2 moved the team flags after the texts and made the colour optional.
void reorderTeam(MutablePacket& packet) {
    io::ByteBuffer in = io::ByteBuffer::wrap(packet.body);
    io::ByteBuffer out;
    protocol::writeString(out, protocol::readString(in));
    const std::uint8_t method = in.readByte();
    out.writeByte(method);
    if (method == 0 || method == 2) {
        copyText(in, out);
        const std::uint8_t flags = in.readByte();
        const std::int32_t visibility = protocol::varInt::read(in);
        const std::int32_t collision = protocol::varInt::read(in);
        const std::int32_t color = protocol::varInt::read(in);
        copyText(in, out);
        copyText(in, out);
        protocol::varInt::write(out, visibility);
        protocol::varInt::write(out, collision);
        protocol::writeBoolean(out, color < kFirstNonColor);
        if (color < kFirstNonColor) {
            protocol::varInt::write(out, color);
        }
        out.writeByte(flags);
    }
    out.writeBytes(in.readable());
    if (!in.failed()) {
        packet.body = out.toVector();
    }
}

} // namespace

bool Link775Up::clientbound(protocol::ProtocolState state, MutablePacket& packet) {
    static const std::int32_t loginPlay = javaTranslators::packetIdAt(
        protocolVersions::kV26_1, protocol::ProtocolState::Play, true, protocol::play::LoginPlayPacket::kId);
    static const std::int32_t kTeam = javaTranslators::packetIdAt(
        protocolVersions::kV26_1, protocol::ProtocolState::Play, true, protocol::play::SetPlayerTeamPacket::kId);
    if (state == protocol::ProtocolState::Play && packet.id == kTeam) {
        reorderTeam(packet);
    }
    if (state == protocol::ProtocolState::Play && packet.id == loginPlay) {
        const LoginFields login = decode(packet, [](io::ByteBuffer& in) { return LoginFields::read(in, false, true); });
        encode(packet, [&](io::ByteBuffer& out) { login.write(out, true, true); });
    }
    return true;
}

} // namespace basalt::wire::multiversion::java
