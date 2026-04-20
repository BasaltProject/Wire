#include <basalt/wire/multiversion/java/Link759.hpp>
#include <basalt/wire/protocol/ProtocolBuffer.hpp>
#include <basalt/wire/protocol/VarInt.hpp>
#include <basalt/wire/protocol/play/PlayPackets.hpp>

#include <cstdint>

namespace basalt::wire::multiversion::java {
namespace {

/// parity: the ids `ChatType` gives its own entries, system and the one above the hotbar.
constexpr std::int32_t kSystem = 1;
constexpr std::int32_t kGameInfo = 2;
constexpr std::int32_t kMaxText = 262144;

} // namespace

bool Link759::clientbound(protocol::ProtocolState state, MutablePacket& packet) {
    if (state != protocol::ProtocolState::Play || packet.nativeId() != protocol::play::SystemChatPacket::kId) {
        return true;
    }
    io::ByteBuffer input = io::ByteBuffer::wrap(packet.body);
    const std::string json = protocol::readString(input, kMaxText);
    const bool overlay = input.readByte() != 0;
    if (input.failed()) {
        return true;
    }
    encode(packet, [&](io::ByteBuffer& output) {
        protocol::writeString(output, json);
        protocol::varInt::write(output, overlay ? kGameInfo : kSystem);
    });
    return true;
}

} // namespace basalt::wire::multiversion::java
