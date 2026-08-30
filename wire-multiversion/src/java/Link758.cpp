#include <basalt/wire/multiversion/java/Link758.hpp>
#include <basalt/wire/protocol/ProtocolBuffer.hpp>
#include <basalt/wire/protocol/VarInt.hpp>
#include <basalt/wire/protocol/play/EntityData.hpp>
#include <basalt/wire/protocol/play/HudPackets.hpp>
#include <basalt/wire/protocol/play/PlayPackets.hpp>
#include <basalt/wire/protocol/play/PlayerStatePackets.hpp>

#include <algorithm>
#include <array>

namespace basalt::wire::multiversion::java {
namespace {

namespace play = protocol::play;
using protocol::ProtocolState;
using Field = Link758::Field;

/// What every version from 1.9 to 1.20.1 numbers the spawn of anything that is not alive.
constexpr std::int32_t kAddEntity = 0x00;

constexpr std::int32_t kAddMob758 = 2;
constexpr std::int32_t kNamedSound758 = 25;
constexpr std::int32_t kChat758 = 15;

constexpr std::int32_t kHolderByName = 0;
constexpr std::int32_t kMaxText = 262144;
constexpr std::int32_t kGameInfo = 2;
constexpr std::uint8_t kSystemPosition = 1;
constexpr std::uint8_t kAboveHotbar = 2;

/// Everything a login says before the death location: 1.19 put it at the very end.
constexpr std::array<Field, 16> kLoginFieldsBeforeDeath = {
    Field::Int,    Field::Byte,   Field::Byte, Field::Byte,   Field::StringArray, Field::Nbt,
    Field::String, Field::String, Field::Long, Field::VarInt, Field::VarInt,      Field::VarInt,
    Field::Byte,   Field::Byte,   Field::Byte, Field::Byte,
};

constexpr std::array<Field, 8> kRespawnFieldsBeforeDeath = {
    Field::String, Field::String, Field::Long, Field::Byte, Field::Byte, Field::Byte, Field::Byte, Field::Byte,
};

/// Everything alive, as 1.19 numbers entity types.
///
/// 1.19 spawns everything with one packet; before it, anything alive had its own. Read out of
/// minecraft-data's own categories for that version.
constexpr std::array<std::int32_t, 79> kLivingIn1_19 = {
    0,  2,  4,  5,   6,   7,   10,  11,  12,  13,  14,  15,  16,  17,  19,  20,  22,  23,  24,  25,
    31, 32, 33, 34,  36,  37,  38,  39,  40,  41,  42,  43,  49,  51,  60,  61,  62,  64,  65,  66,
    67, 68, 69, 70,  71,  73,  74,  75,  76,  77,  78,  80,  81,  82,  83,  85,  88,  89,  90,  91,
    92, 98, 99, 100, 101, 102, 103, 104, 105, 106, 107, 108, 110, 111, 112, 113, 114, 115, 116,
};

} // namespace

bool Link758::clientbound(ProtocolState state, MutablePacket& packet) {
    if (state != ProtocolState::Play) {
        return true;
    }
    switch (packet.nativeId()) {
    case play::SpawnEntityPacket::kId: {
        // A player's spawn is its own packet by here, and its body is not this shape: the link to
        // 1.19.3 already made it one. Only the spawn everything else uses is still numbered zero.
        if (packet.id != kAddEntity) {
            return true;
        }
        io::ByteBuffer input = io::ByteBuffer::wrap(packet.body);
        const std::int32_t entityId = protocol::varInt::read(input);
        const std::int64_t mostSignificant = input.readLong();
        const std::int64_t leastSignificant = input.readLong();
        const std::int32_t type = protocol::varInt::read(input);
        if (std::ranges::find(kLivingIn1_19, type) == kLivingIn1_19.end()) {
            return true;
        }
        const double x = input.readDouble();
        const double y = input.readDouble();
        const double z = input.readDouble();
        const std::uint8_t pitch = input.readByte();
        const std::uint8_t yaw = input.readByte();
        const std::uint8_t headYaw = input.readByte();
        static_cast<void>(protocol::varInt::read(input)); // The object data, which nothing alive uses.
        const std::int16_t velocityX = input.readShort();
        const std::int16_t velocityY = input.readShort();
        const std::int16_t velocityZ = input.readShort();
        if (input.failed()) {
            return true;
        }
        packet.overrideId(kAddMob758);
        encode(packet, [&](io::ByteBuffer& output) {
            protocol::varInt::write(output, entityId);
            output.writeLong(mostSignificant);
            output.writeLong(leastSignificant);
            protocol::varInt::write(output, type);
            output.writeDouble(x);
            output.writeDouble(y);
            output.writeDouble(z);
            // The other way round from the packet everything else spawns with.
            output.writeByte(yaw);
            output.writeByte(pitch);
            output.writeByte(headYaw);
            output.writeShort(velocityX);
            output.writeShort(velocityY);
            output.writeShort(velocityZ);
        });
        break;
    }
    case play::SoundPacket::kId: {
        io::ByteBuffer input = io::ByteBuffer::wrap(packet.body);
        if (protocol::varInt::read(input) != kHolderByName) {
            return false;
        }
        const std::string name = protocol::readString(input);
        static_cast<void>(input.readByte()); // The fixed range, which 1.18.2 has no field for.
        const std::int32_t source = protocol::varInt::read(input);
        const std::int32_t x = input.readInt();
        const std::int32_t y = input.readInt();
        const std::int32_t z = input.readInt();
        const float volume = input.readFloat();
        const float pitch = input.readFloat();
        if (input.failed()) {
            return true;
        }
        packet.overrideId(kNamedSound758);
        encode(packet, [&](io::ByteBuffer& output) {
            protocol::writeString(output, name);
            protocol::varInt::write(output, source);
            output.writeInt(x);
            output.writeInt(y);
            output.writeInt(z);
            output.writeFloat(volume);
            output.writeFloat(pitch);
        });
        break;
    }
    case play::LoginPlayPacket::kId:
        dropDeathLocation(packet, kLoginFieldsBeforeDeath);
        break;
    case play::RespawnPacket::kId:
        dropDeathLocation(packet, kRespawnFieldsBeforeDeath);
        break;
    case play::SystemChatPacket::kId: {
        // The link to 1.19 wrote the type as a number; 1.18.2 wants a position and a sender.
        io::ByteBuffer input = io::ByteBuffer::wrap(packet.body);
        const std::string json = protocol::readString(input, kMaxText);
        const std::int32_t type = protocol::varInt::read(input);
        if (input.failed()) {
            return true;
        }
        packet.overrideId(kChat758);
        encode(packet, [&](io::ByteBuffer& output) {
            protocol::writeString(output, json);
            output.writeByte(type == kGameInfo ? kAboveHotbar : kSystemPosition);
            output.writeLong(0);
            output.writeLong(0);
        });
        break;
    }
    default:
        break;
    }
    return true;
}

void Link758::dropDeathLocation(MutablePacket& packet, std::span<const Field> headFields) {
    io::ByteBuffer input = io::ByteBuffer::wrap(packet.body);
    io::ByteBuffer head;
    for (const Field field : headFields) {
        switch (field) {
        case Field::Int:
            head.writeInt(input.readInt());
            break;
        case Field::Long:
            head.writeLong(input.readLong());
            break;
        case Field::Byte:
            head.writeByte(input.readByte());
            break;
        case Field::VarInt:
            protocol::varInt::write(head, protocol::varInt::read(input));
            break;
        case Field::String:
            protocol::writeString(head, protocol::readString(input));
            break;
        case Field::Nbt:
            protocol::writeAnonymousNbt(head, protocol::readAnonymousNbt(input));
            break;
        case Field::StringArray: {
            const std::int32_t count = protocol::varInt::read(input);
            protocol::varInt::write(head, count);
            for (std::int32_t i = 0; i < count && !input.failed(); ++i) {
                protocol::writeString(head, protocol::readString(input));
            }
            break;
        }
        }
    }
    if (input.failed()) {
        return;
    }
    const std::vector<std::uint8_t> kept = head.toVector();
    encode(packet, [&](io::ByteBuffer& out) { out.writeBytes(kept); });
}

} // namespace basalt::wire::multiversion::java
