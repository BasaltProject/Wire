#include <basalt/wire/multiversion/java/Link340.hpp>
#include <basalt/wire/protocol/ProtocolBuffer.hpp>
#include <basalt/wire/protocol/VarInt.hpp>
#include <basalt/wire/protocol/play/EntityPackets.hpp>
#include <basalt/wire/protocol/play/InventoryPackets.hpp>
#include <basalt/wire/protocol/play/SimplePackets.hpp>
#include <basalt/wire/protocol/play/WorldPackets.hpp>

#include <cstdint>

namespace basalt::wire::multiversion::java {
namespace {

namespace play = protocol::play;
using protocol::ProtocolState;

/// parity: an empty slot is the item id minus one, and nothing follows it.
constexpr std::int32_t kEmptyItem = -1;

constexpr std::int32_t kColumns = 16 * 16;

/// How the tables carry an item and the damage that says which variant of it this is.
constexpr std::int32_t kItemMask = 0xFFFF;
constexpr int kDamageShift = 16;

/// A stack as 1.12.2 reads one: the item in a short, then the count, the damage and its tag.
[[nodiscard]] std::vector<std::uint8_t> oldStack(io::ByteBuffer& input) {
    io::ByteBuffer output;
    if (input.readByte() == 0) {
        output.writeShort(static_cast<std::int16_t>(kEmptyItem));
        return output.toVector();
    }
    const std::int32_t packed = protocol::varInt::read(input);
    const std::uint8_t count = input.readByte();
    const nbt::NbtTag tag = protocol::readAnonymousNbt(input);
    output.writeShort(static_cast<std::int16_t>(packed & kItemMask));
    output.writeByte(count);
    output.writeShort(static_cast<std::int16_t>(static_cast<std::uint32_t>(packed) >> kDamageShift));
    protocol::writeAnonymousNbt(output, tag);
    return output.toVector();
}

/// And back: what the client sends is an id and a damage, and above this it is a name's id.
[[nodiscard]] std::vector<std::uint8_t> newStack(io::ByteBuffer& input) {
    io::ByteBuffer output;
    const std::int32_t itemId = input.readShort();
    if (itemId == kEmptyItem) {
        protocol::writeBoolean(output, false);
        return output.toVector();
    }
    const std::uint8_t count = input.readByte();
    const std::int32_t damage = input.readShort();
    const nbt::NbtTag tag = protocol::readAnonymousNbt(input);
    // Put back together the way the table holds it: what is renumbered above this reads it.
    protocol::writeBoolean(output, true);
    protocol::varInt::write(output, (damage << kDamageShift) | itemId);
    output.writeByte(count);
    protocol::writeAnonymousNbt(output, tag);
    return output.toVector();
}

} // namespace

bool Link340::clientbound(ProtocolState state, MutablePacket& packet) {
    if (state != ProtocolState::Play) {
        return true;
    }
    switch (packet.nativeId()) {
    case play::LevelParticlesPacket::kId:
    case play::AwardStatsPacket::kId:
        return false;
    case play::MapChunkPacket::kId:
        narrowBiomes(packet);
        break;
    case play::ContainerSetSlotPacket::kId: {
        io::ByteBuffer input = io::ByteBuffer::wrap(packet.body);
        const std::uint8_t container = input.readByte();
        const std::int16_t slot = input.readShort();
        const std::vector<std::uint8_t> stack = oldStack(input);
        encode(packet, [&](io::ByteBuffer& output) {
            output.writeByte(container);
            output.writeShort(slot);
            output.writeBytes(stack);
        });
        break;
    }
    case play::ContainerSetContentPacket::kId: {
        io::ByteBuffer input = io::ByteBuffer::wrap(packet.body);
        const std::uint8_t container = input.readByte();
        const std::int16_t count = input.readShort();
        io::ByteBuffer stacks;
        for (std::int16_t i = 0; i < count && !input.failed(); ++i) {
            stacks.writeBytes(oldStack(input));
        }
        const std::vector<std::uint8_t> items = stacks.toVector();
        encode(packet, [&](io::ByteBuffer& output) {
            output.writeByte(container);
            output.writeShort(count);
            output.writeBytes(items);
        });
        break;
    }
    case play::SetEquipmentPacket::kId: {
        io::ByteBuffer input = io::ByteBuffer::wrap(packet.body);
        const std::int32_t entityId = protocol::varInt::read(input);
        const std::int32_t slot = protocol::varInt::read(input);
        const std::vector<std::uint8_t> stack = oldStack(input);
        encode(packet, [&](io::ByteBuffer& output) {
            protocol::varInt::write(output, entityId);
            protocol::varInt::write(output, slot);
            output.writeBytes(stack);
        });
        break;
    }
    default:
        break;
    }
    return true;
}

bool Link340::serverbound(ProtocolState state, MutablePacket& packet) {
    if (state != ProtocolState::Play) {
        return true;
    }
    switch (packet.nativeId()) {
    case play::ServerboundSetCreativeModeSlotPacket::kId: {
        io::ByteBuffer input = io::ByteBuffer::wrap(packet.body);
        const std::int16_t slot = input.readShort();
        const std::vector<std::uint8_t> stack = newStack(input);
        encode(packet, [&](io::ByteBuffer& output) {
            output.writeShort(slot);
            output.writeBytes(stack);
        });
        break;
    }
    case play::ServerboundContainerClickPacket::kId: {
        // 1.12 numbers its clicks itself and sends the stack it thinks it took. This server wants
        // neither: it wants to know whether the click left something on the cursor.
        io::ByteBuffer input = io::ByteBuffer::wrap(packet.body);
        const std::uint8_t container = input.readByte();
        const std::int16_t slot = input.readShort();
        const std::uint8_t button = input.readByte();
        static_cast<void>(input.readShort());
        const std::int32_t mode = input.readByte();
        const bool carried = input.readShort() != kEmptyItem;
        if (input.failed()) {
            return true;
        }
        encode(packet, [&](io::ByteBuffer& output) {
            output.writeByte(container);
            protocol::varInt::write(output, 0);
            output.writeShort(slot);
            output.writeByte(button);
            protocol::varInt::write(output, mode);
            protocol::varInt::write(output, 0);
            protocol::writeBoolean(output, carried);
        });
        break;
    }
    default:
        break;
    }
    return true;
}

void Link340::narrowBiomes(MutablePacket& packet) {
    io::ByteBuffer input = io::ByteBuffer::wrap(packet.body);
    const std::int32_t x = input.readInt();
    const std::int32_t z = input.readInt();
    const std::uint8_t fullChunk = input.readByte();
    const std::int32_t mask = protocol::varInt::read(input);
    const std::int32_t length = protocol::varInt::read(input);
    const std::vector<std::uint8_t> data = input.readBytes(static_cast<std::size_t>(length < 0 ? 0 : length));
    constexpr std::size_t kBiomeBytes = static_cast<std::size_t>(kColumns) * sizeof(std::int32_t);
    if (input.failed() || fullChunk == 0 || data.size() < kBiomeBytes) {
        return;
    }
    const std::size_t sectionBytes = data.size() - kBiomeBytes;
    io::ByteBuffer biomes =
        io::ByteBuffer::wrap({data.begin() + static_cast<std::ptrdiff_t>(sectionBytes), data.end()});
    encode(packet, [&](io::ByteBuffer& output) {
        output.writeInt(x);
        output.writeInt(z);
        output.writeByte(fullChunk);
        protocol::varInt::write(output, mask);
        protocol::varInt::write(output, static_cast<std::int32_t>(sectionBytes) + kColumns);
        output.writeBytes(std::span{data}.first(sectionBytes));
        for (std::int32_t i = 0; i < kColumns; ++i) {
            output.writeByte(static_cast<std::uint8_t>(biomes.readInt()));
        }
        output.writeBytes(input.readable());
    });
}

} // namespace basalt::wire::multiversion::java
