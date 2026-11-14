#include <basalt/wire/multiversion/java/JavaTranslators.hpp>
#include <basalt/wire/multiversion/java/LegacyStacks.hpp>
#include <basalt/wire/multiversion/java/Stacks113.hpp>
#include <basalt/wire/protocol/VarInt.hpp>
#include <basalt/wire/protocol/play/EntityPackets.hpp>
#include <basalt/wire/protocol/play/InventoryPackets.hpp>

namespace basalt::wire::multiversion::java {
namespace {

namespace play = protocol::play;
using protocol::ProtocolState;

template <typename Reshape>
void reshape(MutablePacket& packet, Reshape&& body) {
    io::ByteBuffer input = io::ByteBuffer::wrap(packet.body);
    io::ByteBuffer output;
    if (body(input, output) && !input.failed()) {
        output.writeBytes(input.readable());
        packet.body = output.toVector();
    }
}

} // namespace

Stacks113::Stacks113(std::int32_t protocol, PacketContentRewriter& next)
    : next_(&next), creativeSlotId_(javaTranslators::packetIdAt(protocol, ProtocolState::Play, false,
                                                                play::ServerboundSetCreativeModeSlotPacket::kId)),
      editBookId_(
          javaTranslators::packetIdAt(protocol, ProtocolState::Play, false, play::ServerboundEditBookPacket::kId)) {}

bool Stacks113::clientbound(ProtocolState state, MutablePacket& packet) {
    if (!next_->clientbound(state, packet)) {
        return false;
    }
    if (state != ProtocolState::Play) {
        return true;
    }
    switch (packet.nativeId()) {
    case play::ContainerSetSlotPacket::kId:
        reshape(packet, [](io::ByteBuffer& in, io::ByteBuffer& out) {
            out.writeByte(in.readByte());
            out.writeShort(in.readShort());
            return legacyStacks::flaggedToShortId(in, out);
        });
        break;
    case play::ContainerSetContentPacket::kId:
        reshape(packet, [](io::ByteBuffer& in, io::ByteBuffer& out) {
            out.writeByte(in.readByte());
            const std::int16_t count = in.readShort();
            out.writeShort(count);
            for (std::int16_t i = 0; i < count; ++i) {
                if (!legacyStacks::flaggedToShortId(in, out)) {
                    return false;
                }
            }
            return true;
        });
        break;
    case play::SetEquipmentPacket::kId:
        reshape(packet, [](io::ByteBuffer& in, io::ByteBuffer& out) {
            protocol::varInt::write(out, protocol::varInt::read(in));
            protocol::varInt::write(out, protocol::varInt::read(in));
            return legacyStacks::flaggedToShortId(in, out);
        });
        break;
    default:
        break;
    }
    return true;
}

bool Stacks113::serverbound(ProtocolState state, MutablePacket& packet) {
    if (!next_->serverbound(state, packet)) {
        return false;
    }
    if (state == ProtocolState::Play && packet.id == creativeSlotId_) {
        reshape(packet, [](io::ByteBuffer& in, io::ByteBuffer& out) {
            out.writeShort(in.readShort());
            return legacyStacks::shortIdToFlagged(in, out);
        });
    } else if (state == ProtocolState::Play && packet.id == editBookId_) {
        reshape(packet,
                [](io::ByteBuffer& in, io::ByteBuffer& out) { return legacyStacks::shortIdToFlagged(in, out); });
    }
    return true;
}

} // namespace basalt::wire::multiversion::java
