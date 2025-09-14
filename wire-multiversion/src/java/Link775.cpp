#include <basalt/wire/multiversion/java/Link775.hpp>
#include <basalt/wire/multiversion/java/LoginFields.hpp>
#include <basalt/wire/protocol/ProtocolBuffer.hpp>
#include <basalt/wire/protocol/VarInt.hpp>
#include <basalt/wire/protocol/login/LoginPackets.hpp>
#include <basalt/wire/protocol/play/PlayPackets.hpp>

namespace basalt::wire::multiversion::java {

bool Link775::clientbound(protocol::ProtocolState state, MutablePacket& packet) {
    if (state == protocol::ProtocolState::Play && packet.nativeId() == protocol::play::LoginPlayPacket::kId) {
        const LoginFields login = decode(packet, [](io::ByteBuffer& in) { return LoginFields::read(in, true, true); });
        encode(packet, [&](io::ByteBuffer& out) { login.write(out, false, true); });
        return true;
    }
    if (state != protocol::ProtocolState::Login || packet.nativeId() != protocol::login::LoginFinishedPacket::kId) {
        return true;
    }
    rewrite(packet, [](io::ByteBuffer& input, io::ByteBuffer& output) {
        // The profile, field by field: there is no session id on the other side to write.
        output.writeLong(input.readLong());
        output.writeLong(input.readLong());
        protocol::writeString(output, protocol::readString(input, 16));
        const std::int32_t properties = protocol::varInt::read(input);
        protocol::varInt::write(output, properties);
        for (std::int32_t i = 0; i < properties && !input.failed(); ++i) {
            protocol::writeString(output, protocol::readString(input));
            protocol::writeString(output, protocol::readString(input));
            const std::uint8_t signed_ = input.readByte();
            output.writeByte(signed_);
            if (signed_ != 0) {
                protocol::writeString(output, protocol::readString(input));
            }
        }
    });
    return true;
}

} // namespace basalt::wire::multiversion::java
