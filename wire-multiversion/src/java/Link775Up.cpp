#include <basalt/wire/multiversion/java/Link775Up.hpp>
#include <basalt/wire/multiversion/ProtocolVersions.hpp>
#include <basalt/wire/multiversion/java/JavaTranslators.hpp>
#include <basalt/wire/multiversion/java/LoginFields.hpp>
#include <basalt/wire/protocol/play/PlayPackets.hpp>

namespace basalt::wire::multiversion::java {

bool Link775Up::clientbound(protocol::ProtocolState state, MutablePacket& packet) {
    static const std::int32_t loginPlay = javaTranslators::packetIdAt(
        protocolVersions::kV26_1, protocol::ProtocolState::Play, true, protocol::play::LoginPlayPacket::kId);
    if (state == protocol::ProtocolState::Play && packet.id == loginPlay) {
        const LoginFields login = decode(packet, [](io::ByteBuffer& in) { return LoginFields::read(in, false, true); });
        encode(packet, [&](io::ByteBuffer& out) { login.write(out, true, true); });
    }
    return true;
}

} // namespace basalt::wire::multiversion::java
