#include <basalt/wire/multiversion/java/Link755.hpp>
#include <basalt/wire/protocol/ProtocolBuffer.hpp>
#include <basalt/wire/protocol/VarInt.hpp>
#include <basalt/wire/protocol/play/InventoryPackets.hpp>
#include <basalt/wire/protocol/play/PlayerStatePackets.hpp>

#include <algorithm>
#include <cstdint>
#include <vector>

namespace basalt::wire::multiversion::java {
namespace {

namespace play = protocol::play;

constexpr std::int32_t kRemoveEntity755 = 58;

/// One stack as everything below 1.20.5 writes it, copied across without looking inside.
void copyStack(io::ByteBuffer& input, io::ByteBuffer& output) {
    const std::uint8_t present = input.readByte();
    output.writeByte(present);
    if (present == 0) {
        return;
    }
    protocol::varInt::write(output, protocol::varInt::read(input));
    output.writeByte(input.readByte());
    protocol::writeAnonymousNbt(output, protocol::readAnonymousNbt(input));
}

} // namespace

bool Link755::clientbound(protocol::ProtocolState state, MutablePacket& packet) {
    if (state != protocol::ProtocolState::Play) {
        return true;
    }
    switch (packet.nativeId()) {
    case play::RemoveEntitiesPacket::kId: {
        io::ByteBuffer input = io::ByteBuffer::wrap(packet.body);
        const std::int32_t count = protocol::varInt::read(input);
        std::vector<std::int32_t> ids;
        ids.reserve(std::min(static_cast<std::size_t>(count < 0 ? 0 : count), input.readableBytes()));
        for (std::int32_t i = 0; i < count && !input.failed(); ++i) {
            ids.push_back(protocol::varInt::read(input));
        }
        if (ids.empty()) {
            return false;
        }
        packet.overrideId(kRemoveEntity755);
        encode(packet, [&](io::ByteBuffer& out) { protocol::varInt::write(out, ids.front()); });
        for (std::size_t i = 1; i < ids.size(); ++i) {
            io::ByteBuffer one;
            protocol::varInt::write(one, ids[i]);
            packet.extras.emplace_back(kRemoveEntity755, one.toVector());
        }
        break;
    }
    case play::ContainerSetSlotPacket::kId: {
        io::ByteBuffer input = io::ByteBuffer::wrap(packet.body);
        const std::uint8_t containerId = input.readByte();
        static_cast<void>(protocol::varInt::read(input));
        encode(packet, [&](io::ByteBuffer& output) {
            output.writeByte(containerId);
            output.writeBytes(input.readable());
        });
        break;
    }
    case play::ContainerSetContentPacket::kId: {
        io::ByteBuffer input = io::ByteBuffer::wrap(packet.body);
        const std::uint8_t containerId = input.readByte();
        static_cast<void>(protocol::varInt::read(input));
        const std::int32_t count = protocol::varInt::read(input);
        io::ByteBuffer items;
        for (std::int32_t i = 0; i < count && !input.failed(); ++i) {
            copyStack(input, items);
        }
        const std::vector<std::uint8_t> slots = items.toVector();
        encode(packet, [&](io::ByteBuffer& output) {
            output.writeByte(containerId);
            output.writeShort(static_cast<std::int16_t>(count));
            output.writeBytes(slots);
        });
        break;
    }
    default:
        break;
    }
    return true;
}

} // namespace basalt::wire::multiversion::java
