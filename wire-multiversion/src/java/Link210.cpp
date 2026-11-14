#include <basalt/wire/multiversion/java/Link210.hpp>
#include <basalt/wire/multiversion/java/NamedBlockEntities.hpp>
#include <basalt/wire/multiversion/java/NamedNbt.hpp>
#include <basalt/wire/protocol/VarInt.hpp>
#include <basalt/wire/protocol/play/BlockEntityPackets.hpp>
#include <basalt/wire/protocol/play/EntityData.hpp>
#include <basalt/wire/protocol/play/EntityPackets.hpp>
#include <basalt/wire/protocol/play/HudPackets.hpp>
#include <basalt/wire/protocol/play/PlayPackets.hpp>
#include <basalt/wire/protocol/play/WorldPackets.hpp>

#include <array>
#include <cstdint>

namespace basalt::wire::multiversion::java {
namespace {

namespace play = protocol::play;

constexpr float kSixteenths = 16.0F;

constexpr std::int32_t kAddMob = 0x03;

/// parity: `ClientboundSetTitlesPacket.Type`.
constexpr std::int32_t kActionBar = 2;

void renameChunkEntities(MutablePacket& packet) {
    io::ByteBuffer input = io::ByteBuffer::wrap(packet.body);
    io::ByteBuffer output;
    output.writeInt(input.readInt());
    output.writeInt(input.readInt());
    output.writeByte(input.readByte());
    protocol::varInt::write(output, protocol::varInt::read(input));
    const std::int32_t length = protocol::varInt::read(input);
    protocol::varInt::write(output, length);
    output.writeBytes(input.readSpan(static_cast<std::size_t>(length < 0 ? 0 : length)));
    const std::int32_t count = protocol::varInt::read(input);
    protocol::varInt::write(output, count);
    for (std::int32_t i = 0; i < count && !input.failed(); ++i) {
        nbt::NbtTag tag = readNamedTag(input);
        if (auto* entity = tag.get<nbt::NbtCompound>()) {
            namedBlockEntities::useNameBefore1_11(*entity);
        }
        writeNamedNbt(output, tag);
    }
    if (!input.failed()) {
        packet.body = output.toVector();
    }
}

} // namespace

bool Link210::clientbound(protocol::ProtocolState state, MutablePacket& packet) {
    if (state != protocol::ProtocolState::Play) {
        return true;
    }
    switch (packet.nativeId()) {
    case play::MapChunkPacket::kId:
        renameChunkEntities(packet);
        break;
    case play::BlockEntityDataPacket::kId: {
        io::ByteBuffer input = io::ByteBuffer::wrap(packet.body);
        const std::int64_t pos = input.readLong();
        const std::uint8_t action = input.readByte();
        nbt::NbtTag tag = readNamedTag(input);
        if (auto* entity = tag.get<nbt::NbtCompound>(); entity != nullptr && !input.failed()) {
            namedBlockEntities::useNameBefore1_11(*entity);
            encode(packet, [&](io::ByteBuffer& out) {
                out.writeLong(pos);
                out.writeByte(action);
                writeNamedNbt(out, tag);
            });
        }
        break;
    }
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

bool Link210::serverbound(protocol::ProtocolState state, MutablePacket& packet) {
    if (state != protocol::ProtocolState::Play || packet.nativeId() != play::ServerboundUseItemOnPacket::kId) {
        return true;
    }
    // Where on the face the click landed went from sixteenths in a byte to a float with 1.11.
    io::ByteBuffer input = io::ByteBuffer::wrap(packet.body);
    const std::int64_t pos = input.readLong();
    const std::int32_t face = protocol::varInt::read(input);
    const std::int32_t hand = protocol::varInt::read(input);
    std::array<float, 3> cursor{};
    for (float& axis : cursor) {
        axis = static_cast<float>(input.readByte()) / kSixteenths;
    }
    if (input.failed()) {
        return false;
    }
    encode(packet, [&](io::ByteBuffer& output) {
        output.writeLong(pos);
        protocol::varInt::write(output, face);
        protocol::varInt::write(output, hand);
        for (const float axis : cursor) {
            output.writeFloat(axis);
        }
    });
    return true;
}

} // namespace basalt::wire::multiversion::java
