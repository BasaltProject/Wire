#include <basalt/wire/multiversion/java/SafeInt.hpp>
#include <basalt/wire/multiversion/java/Link110.hpp>
#include <basalt/wire/protocol/ProtocolBuffer.hpp>
#include <basalt/wire/protocol/VarInt.hpp>
#include <basalt/wire/protocol/play/HudPackets.hpp>

#include <algorithm>
#include <cstdint>
#include <string>

namespace basalt::wire::multiversion::java {
namespace {

constexpr std::int32_t kNamedSound = 0x19;

constexpr float kPitchSteps = 63.0F;

} // namespace

bool Link110::clientbound(protocol::ProtocolState state, MutablePacket& packet) {
    if (state != protocol::ProtocolState::Play || packet.nativeId() != protocol::play::SoundPacket::kId) {
        return true;
    }
    io::ByteBuffer input = io::ByteBuffer::wrap(packet.body);
    const bool named = packet.id == kNamedSound;
    const std::string name = named ? protocol::readString(input) : std::string{};
    const std::int32_t sound = named ? 0 : protocol::varInt::read(input);
    const std::int32_t category = protocol::varInt::read(input);
    const std::int32_t x = input.readInt();
    const std::int32_t y = input.readInt();
    const std::int32_t z = input.readInt();
    const float volume = input.readFloat();
    const float pitch = input.readFloat();
    if (input.failed()) {
        return true;
    }
    encode(packet, [&](io::ByteBuffer& output) {
        if (named) {
            protocol::writeString(output, name);
        } else {
            protocol::varInt::write(output, sound);
        }
        protocol::varInt::write(output, category);
        output.writeInt(x);
        output.writeInt(y);
        output.writeInt(z);
        output.writeFloat(volume);
        output.writeByte(static_cast<std::uint8_t>(std::clamp<std::int64_t>(safeInt(pitch * kPitchSteps), 0, 255)));
    });
    return true;
}

} // namespace basalt::wire::multiversion::java
