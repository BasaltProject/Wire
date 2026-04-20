#include <basalt/wire/multiversion/java/Link210.hpp>
#include <basalt/wire/protocol/VarInt.hpp>
#include <basalt/wire/protocol/play/EntityData.hpp>
#include <basalt/wire/protocol/play/EntityPackets.hpp>
#include <basalt/wire/protocol/play/HudPackets.hpp>

#include <cstdint>

namespace basalt::wire::multiversion::java {
namespace {

namespace play = protocol::play;

constexpr std::int32_t kAddMob = 0x03;

/// parity: `ClientboundSetTitlesPacket.Type`.
constexpr std::int32_t kActionBar = 2;

} // namespace

bool Link210::clientbound(protocol::ProtocolState state, MutablePacket& packet) {
    if (state != protocol::ProtocolState::Play) {
        return true;
    }
    switch (packet.nativeId()) {
    case play::SpawnEntityPacket::kId: {
        // Only a mob is affected: the link to 1.18.2 gave it the packet 1.11 calls its own.
        if (packet.id != kAddMob) {
            return true;
        }
        io::ByteBuffer input = io::ByteBuffer::wrap(packet.body);
        const std::int32_t entityId = protocol::varInt::read(input);
        const std::int64_t mostSignificant = input.readLong();
        const std::int64_t leastSignificant = input.readLong();
        const std::int32_t type = protocol::varInt::read(input);
        encode(packet, [&](io::ByteBuffer& output) {
            protocol::varInt::write(output, entityId);
            output.writeLong(mostSignificant);
            output.writeLong(leastSignificant);
            output.writeByte(static_cast<std::uint8_t>(type));
            output.writeBytes(input.readable());
        });
        break;
    }
    case play::TakeItemEntityPacket::kId: {
        io::ByteBuffer input = io::ByteBuffer::wrap(packet.body);
        const std::int32_t collected = protocol::varInt::read(input);
        const std::int32_t collector = protocol::varInt::read(input);
        encode(packet, [&](io::ByteBuffer& output) {
            protocol::varInt::write(output, collected);
            protocol::varInt::write(output, collector);
        });
        break;
    }
    case play::kSetActionBarTextId:
        return false;
    case play::kSetTitleTextId:
    case play::kSetSubtitleTextId:
    case play::SetTitlesAnimationPacket::kId:
    case play::ClearTitlesPacket::kId: {
        io::ByteBuffer input = io::ByteBuffer::wrap(packet.body);
        const std::int32_t action = protocol::varInt::read(input);
        encode(packet, [&](io::ByteBuffer& output) {
            protocol::varInt::write(output, action > kActionBar ? action - 1 : action);
            output.writeBytes(input.readable());
        });
        break;
    }
    default:
        break;
    }
    return true;
}

} // namespace basalt::wire::multiversion::java
