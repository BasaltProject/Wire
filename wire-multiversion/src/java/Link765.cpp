#include <basalt/wire/multiversion/java/LegacyStacks.hpp>
#include <basalt/wire/multiversion/java/Link765.hpp>
#include <basalt/wire/protocol/ProtocolBuffer.hpp>
#include <basalt/wire/protocol/VarInt.hpp>
#include <basalt/wire/protocol/play/EntityPackets.hpp>
#include <basalt/wire/protocol/play/InventoryPackets.hpp>

#include <string>
#include <vector>

namespace basalt::wire::multiversion::java {
namespace {

namespace play = protocol::play;
using protocol::ProtocolState;

/// The high bit of an equipment slot, saying another one follows.
constexpr std::int32_t kContinues = 0x80;

} // namespace

bool Link765::clientbound(ProtocolState state, MutablePacket& packet) {
    if (state != ProtocolState::Play) {
        return true;
    }
    switch (packet.nativeId()) {
    case play::UpdateAttributesPacket::kId:
        nameAttributes(packet);
        break;
    case play::ContainerSetSlotPacket::kId: {
        // The link to 1.21 already put the container in one byte, so this reads that.
        io::ByteBuffer input = io::ByteBuffer::wrap(packet.body);
        const std::uint8_t container = input.readByte();
        const std::int32_t stateId = protocol::varInt::read(input);
        const std::int16_t slot = input.readShort();
        const std::optional<play::WireItemStack> item = play::WireItemStack::read(input);
        encode(packet, [&](io::ByteBuffer& output) {
            output.writeByte(container);
            protocol::varInt::write(output, stateId);
            output.writeShort(slot);
            legacyStacks::write(output, item, older());
        });
        break;
    }
    case play::ContainerSetContentPacket::kId: {
        io::ByteBuffer input = io::ByteBuffer::wrap(packet.body);
        const std::uint8_t container = input.readByte();
        const std::int32_t stateId = protocol::varInt::read(input);
        const std::int32_t count = protocol::varInt::read(input);
        std::vector<std::optional<play::WireItemStack>> items;
        items.reserve(static_cast<std::size_t>(count < 0 ? 0 : count));
        for (std::int32_t i = 0; i < count && !input.failed(); ++i) {
            items.push_back(play::WireItemStack::read(input));
        }
        const std::optional<play::WireItemStack> carried = play::WireItemStack::read(input);
        encode(packet, [&](io::ByteBuffer& output) {
            output.writeByte(container);
            protocol::varInt::write(output, stateId);
            protocol::varInt::write(output, static_cast<std::int32_t>(items.size()));
            for (const std::optional<play::WireItemStack>& item : items) {
                legacyStacks::write(output, item, older());
            }
            legacyStacks::write(output, carried, older());
        });
        break;
    }
    case play::SetEquipmentPacket::kId: {
        const auto equipment = decode(packet, [](io::ByteBuffer& in) { return play::SetEquipmentPacket::read(in); });
        if (!equipment.has_value()) {
            return true;
        }
        encode(packet, [&](io::ByteBuffer& output) {
            protocol::varInt::write(output, equipment->entityId);
            for (std::size_t index = 0; index < equipment->slots.size(); ++index) {
                const play::SetEquipmentPacket::Slot& slot = equipment->slots[index];
                const auto marker = static_cast<std::int32_t>(slot.type);
                const bool last = index + 1 == equipment->slots.size();
                output.writeByte(static_cast<std::uint8_t>(last ? marker : marker | kContinues));
                legacyStacks::write(output, slot.stack, older());
            }
        });
        break;
    }
    default:
        break;
    }
    return true;
}

bool Link765::serverbound(ProtocolState state, MutablePacket& packet) {
    if (state != ProtocolState::Play || packet.nativeId() != play::ServerboundSetCreativeModeSlotPacket::kId) {
        return true;
    }
    // The stack coming back is the old shape, and what reads it next is the new one.
    io::ByteBuffer input = io::ByteBuffer::wrap(packet.body);
    const std::int16_t slot = input.readShort();
    if (input.readByte() == 0) {
        encode(packet, [&](io::ByteBuffer& output) {
            output.writeShort(slot);
            protocol::varInt::write(output, 0);
        });
        return true;
    }
    const std::int32_t itemId = protocol::varInt::read(input);
    const std::int32_t count = input.readByte();
    encode(packet, [&](io::ByteBuffer& output) {
        output.writeShort(slot);
        protocol::varInt::write(output, count);
        protocol::varInt::write(output, itemId);
        // The tag is dropped rather than guessed at: what it held is components over here, and the
        // names do not line up. What the item is and how many survive.
        protocol::varInt::write(output, 0);
        protocol::varInt::write(output, 0);
    });
    return true;
}

void Link765::nameAttributes(MutablePacket& packet) const {
    io::ByteBuffer input = io::ByteBuffer::wrap(packet.body);
    const std::int32_t entityId = protocol::varInt::read(input);

    struct Modifier {
        common::Uuid uuid;
        double amount = 0.0;
        std::uint8_t operation = 0;
    };

    struct Attribute {
        std::string name;
        double base = 0.0;
        std::vector<Modifier> modifiers;
    };

    std::vector<Attribute> named;
    const std::int32_t count = protocol::varInt::read(input);
    for (std::int32_t i = 0; i < count && !input.failed(); ++i) {
        const std::int32_t id = protocol::varInt::read(input);
        Attribute attribute;
        attribute.base = input.readDouble();
        const std::int32_t modifiers = protocol::varInt::read(input);
        for (std::int32_t j = 0; j < modifiers && !input.failed(); ++j) {
            Modifier modifier;
            modifier.uuid = protocol::readUuid(input);
            modifier.amount = input.readDouble();
            modifier.operation = input.readByte();
            attribute.modifiers.push_back(modifier);
        }
        const std::string_view name = target_->attributeNames().name(id);
        if (name.empty()) {
            continue;
        }
        attribute.name = name;
        named.push_back(std::move(attribute));
    }
    if (input.failed()) {
        return;
    }
    encode(packet, [&](io::ByteBuffer& output) {
        protocol::varInt::write(output, entityId);
        protocol::varInt::write(output, static_cast<std::int32_t>(named.size()));
        for (const Attribute& attribute : named) {
            protocol::writeString(output, attribute.name);
            output.writeDouble(attribute.base);
            protocol::varInt::write(output, static_cast<std::int32_t>(attribute.modifiers.size()));
            for (const Modifier& modifier : attribute.modifiers) {
                protocol::writeUuid(output, modifier.uuid);
                output.writeDouble(modifier.amount);
                output.writeByte(modifier.operation);
            }
        }
    });
}

} // namespace basalt::wire::multiversion::java
