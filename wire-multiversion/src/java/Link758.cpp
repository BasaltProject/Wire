#include <basalt/wire/multiversion/java/ChatShapes.hpp>
#include <basalt/wire/multiversion/java/LegacyStacks.hpp>
#include <basalt/wire/multiversion/java/Link758.hpp>
#include <basalt/wire/multiversion/java/MerchantShapes.hpp>
#include <basalt/wire/multiversion/java/NamedNbt.hpp>
#include <basalt/wire/protocol/ProtocolBuffer.hpp>
#include <basalt/wire/protocol/VarInt.hpp>
#include <basalt/wire/protocol/play/BlockPos.hpp>
#include <basalt/wire/protocol/play/EntityData.hpp>
#include <basalt/wire/protocol/play/EntityPackets.hpp>
#include <basalt/wire/protocol/play/HudPackets.hpp>
#include <basalt/wire/protocol/play/InventoryPackets.hpp>
#include <basalt/wire/protocol/play/PlayPackets.hpp>
#include <basalt/wire/protocol/play/PlayerStatePackets.hpp>

#include <algorithm>
#include <array>
#include <cstdint>
#include <string>

namespace basalt::wire::multiversion::java {
namespace {

namespace play = protocol::play;
using protocol::ProtocolState;
using Field = Link758::Field;

constexpr std::int32_t kAddEntity = 0x00;
constexpr std::size_t kUuidBytes = 16;

// parity: the names of brigadier's own parsers, the six every version numbers alike.
/// parity: ClientboundPlayerInfoPacket.Action.ADD_PLAYER.
constexpr std::int32_t kAddPlayerAction = 0;
constexpr std::size_t kPositionBytes = 3 * sizeof(double);

constexpr std::int32_t kAddMob758 = 2;
constexpr std::int32_t kAddPainting758 = 3;
constexpr std::size_t kSpawnAngleBytes = 3;
constexpr std::size_t kLoginModeBytes = 3;
/// parity: `Direction.get2DDataValue`, by each horizontal direction's 3D value: south, west, north, east.
constexpr std::array<std::int32_t, 4> kHorizontalFacings = {3, 4, 2, 5};
constexpr std::int32_t kNamedSound758 = 25;
constexpr std::int32_t kChat758 = 15;

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

/// parity: `ClientboundAddPaintingPacket`; the entity type carries the motive and the data its 3D facing.
void toPainting(MutablePacket& packet) {
    io::ByteBuffer input = io::ByteBuffer::wrap(packet.body);
    const std::int32_t entityId = protocol::varInt::read(input);
    const std::int64_t mostSignificant = input.readLong();
    const std::int64_t leastSignificant = input.readLong();
    const std::int32_t motive = protocol::varInt::read(input);
    const play::BlockPos pos{static_cast<std::int32_t>(input.readDouble()),
                             static_cast<std::int32_t>(input.readDouble()),
                             static_cast<std::int32_t>(input.readDouble())};
    input.skip(kSpawnAngleBytes);
    const std::int32_t facing = protocol::varInt::read(input);
    if (input.failed()) {
        return;
    }
    const auto horizontal = std::ranges::find(kHorizontalFacings, facing);
    io::ByteBuffer output;
    protocol::varInt::write(output, entityId);
    output.writeLong(mostSignificant);
    output.writeLong(leastSignificant);
    protocol::varInt::write(output, motive);
    output.writeLong(pos.pack());
    output.writeByte(horizontal == kHorizontalFacings.end()
                         ? 0
                         : static_cast<std::uint8_t>(horizontal - kHorizontalFacings.begin()));
    packet.body = output.toVector();
    packet.overrideId(kAddPainting758);
}

/// 1.19 added each new player's chat key after its display name; nothing before it reads one.
void dropProfileKeys(MutablePacket& packet) {
    io::ByteBuffer input = io::ByteBuffer::wrap(packet.body);
    if (protocol::varInt::read(input) != kAddPlayerAction) {
        return;
    }
    io::ByteBuffer output;
    protocol::varInt::write(output, kAddPlayerAction);
    const std::int32_t entries = protocol::varInt::read(input);
    protocol::varInt::write(output, entries);
    for (std::int32_t i = 0; i < entries && !input.failed(); ++i) {
        output.writeBytes(input.readSpan(kUuidBytes));
        protocol::writeString(output, protocol::readString(input));
        const std::int32_t properties = protocol::varInt::read(input);
        protocol::varInt::write(output, properties);
        for (std::int32_t k = 0; k < properties && !input.failed(); ++k) {
            protocol::writeString(output, protocol::readString(input));
            protocol::writeString(output, protocol::readString(input));
            const bool signature = protocol::readBoolean(input);
            protocol::writeBoolean(output, signature);
            if (signature) {
                protocol::writeString(output, protocol::readString(input));
            }
        }
        protocol::varInt::write(output, protocol::varInt::read(input));
        protocol::varInt::write(output, protocol::varInt::read(input));
        const bool display = protocol::readBoolean(input);
        protocol::writeBoolean(output, display);
        if (display) {
            protocol::writeString(output, protocol::readString(input));
        }
        if (protocol::readBoolean(input)) {
            static_cast<void>(input.readLong());
            input.skip(static_cast<std::size_t>(std::max(protocol::varInt::read(input), 0)));
            input.skip(static_cast<std::size_t>(std::max(protocol::varInt::read(input), 0)));
        }
    }
    if (!input.failed()) {
        packet.body = output.toVector();
    }
}

/// Before 1.19 an argument parser went by name; the ones left by now are brigadier's, numbered from zero.
} // namespace

bool Link758::clientbound(ProtocolState state, MutablePacket& packet) {
    if (state != ProtocolState::Play) {
        return true;
    }
    switch (packet.nativeId()) {
    case play::LevelParticlesPacket::kId:
        // Before 1.19 the particle was an int.
        rewrite(packet, [](io::ByteBuffer& input, io::ByteBuffer& output) {
            output.writeInt(protocol::varInt::read(input));
            output.writeBytes(input.readable());
        });
        break;
    case play::MerchantOffersPacket::kId:
        merchantShapes::reshape(packet, merchantShapes::k1_19, merchantShapes::k1_14_4, &legacyStacks::copyNamed);
        break;
    case play::PlayerInfoUpdatePacket::kId:
        dropProfileKeys(packet);
        break;
    case play::BlockChangedAckPacket::kId:
        return false;
    case play::UpdateMobEffectPacket::kId:
        // Before 1.19 the effect was one byte and nothing followed the flags.
        rewrite(packet, [](io::ByteBuffer& input, io::ByteBuffer& output) {
            protocol::varInt::write(output, protocol::varInt::read(input));
            output.writeByte(static_cast<std::uint8_t>(protocol::varInt::read(input)));
            output.writeBytes(input.readSpan(1));
            protocol::varInt::write(output, protocol::varInt::read(input));
            output.writeBytes(input.readSpan(1));
        });
        break;
    case play::SpawnEntityPacket::kId: {
        // Only the spawn everything else uses is still numbered zero.
        if (packet.id != kAddEntity) {
            return true;
        }
        io::ByteBuffer input = io::ByteBuffer::wrap(packet.body);
        const std::int32_t entityId = protocol::varInt::read(input);
        const std::int64_t mostSignificant = input.readLong();
        const std::int64_t leastSignificant = input.readLong();
        const std::int32_t type = protocol::varInt::read(input);
        if (packet.spawn == MutablePacket::Spawn::kPainting) {
            toPainting(packet);
            return true;
        }
        if (packet.spawn != MutablePacket::Spawn::kMob) {
            // An object before 1.19 had no head rotation and its data was a plain int.
            rewrite(packet, [](io::ByteBuffer& in, io::ByteBuffer& out) {
                protocol::varInt::write(out, protocol::varInt::read(in));
                out.writeBytes(in.readSpan(kUuidBytes));
                protocol::varInt::write(out, protocol::varInt::read(in));
                out.writeBytes(in.readSpan(kPositionBytes + 2));
                static_cast<void>(in.readByte());
                out.writeInt(protocol::varInt::read(in));
                out.writeBytes(in.readable());
            });
            return true;
        }
        const double x = input.readDouble();
        const double y = input.readDouble();
        const double z = input.readDouble();
        const std::uint8_t pitch = input.readByte();
        const std::uint8_t yaw = input.readByte();
        const std::uint8_t headYaw = input.readByte();
        static_cast<void>(protocol::varInt::read(input));
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
        // parity: `ClientboundCustomSoundPacket` before 1.19 carried no seed.
        io::ByteBuffer input = io::ByteBuffer::wrap(packet.body);
        const std::string name = protocol::readString(input);
        const std::int32_t source = protocol::varInt::read(input);
        const std::int32_t x = input.readInt();
        const std::int32_t y = input.readInt();
        const std::int32_t z = input.readInt();
        const float volume = input.readFloat();
        const float pitch = input.readFloat();
        static_cast<void>(input.readLong());
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
        dimensionInLogin(packet);
        break;
    case play::RespawnPacket::kId:
        dropDeathLocation(packet, kRespawnFieldsBeforeDeath);
        dimensionInRespawn(packet);
        break;
    case play::SystemChatPacket::kId: {
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
        case Field::Nbt: {
            // 1.19 brought the chat types into the codec; a client before it has no such registry.
            nbt::NbtTag codec = readNamedTag(input);
            if (auto* registries = codec.get<nbt::NbtCompound>()) {
                registries->remove("minecraft:chat_type");
            }
            writeNamedNbt(head, codec);
            break;
        }
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

void Link758::dimensionInLogin(MutablePacket& packet) {
    io::ByteBuffer input = io::ByteBuffer::wrap(packet.body);
    io::ByteBuffer output;
    output.writeInt(input.readInt());
    output.writeBytes(input.readSpan(kLoginModeBytes));
    const std::int32_t count = protocol::varInt::read(input);
    protocol::varInt::write(output, count);
    for (std::int32_t i = 0; i < count && !input.failed(); ++i) {
        protocol::writeString(output, protocol::readString(input));
    }
    const nbt::NbtTag codec = readNamedTag(input);
    remember(codec);
    writeNamedNbt(output, codec);
    writeDimension(output, protocol::readString(input));
    output.writeBytes(input.readable());
    if (!input.failed()) {
        packet.body = output.toVector();
    }
}

void Link758::dimensionInRespawn(MutablePacket& packet) const {
    io::ByteBuffer input = io::ByteBuffer::wrap(packet.body);
    const std::string dimensionType = protocol::readString(input);
    if (input.failed()) {
        return;
    }
    encode(packet, [&](io::ByteBuffer& output) {
        writeDimension(output, dimensionType);
        output.writeBytes(input.readable());
    });
}

void Link758::remember(const nbt::NbtTag& codec) {
    const nbt::NbtCompound* root = codec.get<nbt::NbtCompound>();
    const nbt::NbtCompound* registry = root != nullptr ? root->getCompound("minecraft:dimension_type") : nullptr;
    const nbt::NbtList* values = registry != nullptr ? registry->getList("value") : nullptr;
    if (values == nullptr) {
        return;
    }
    dimensions_.clear();
    for (const nbt::NbtTag& entry : *values) {
        const nbt::NbtCompound* wrapped = entry.get<nbt::NbtCompound>();
        const nbt::NbtTag* name = wrapped != nullptr ? wrapped->find("name") : nullptr;
        const nbt::NbtTag* element = wrapped != nullptr ? wrapped->find("element") : nullptr;
        if (name == nullptr || element == nullptr) {
            continue;
        }
        if (const auto* text = name->get<std::string>()) {
            dimensions_.emplace(*text, *element);
        }
    }
}

void Link758::writeDimension(io::ByteBuffer& output, const std::string& name) const {
    const auto found = dimensions_.find(name);
    writeNamedNbt(output, found == dimensions_.end() ? nbt::NbtTag::ofCompound(nbt::NbtCompound{}) : found->second);
}

bool Link758::serverbound(protocol::ProtocolState state, MutablePacket& packet) {
    if (state != protocol::ProtocolState::Play) {
        return true;
    }
    switch (packet.nativeId()) {
    case play::ServerboundPlayerActionPacket::kId:
    case play::ServerboundUseItemOnPacket::kId:
    case play::ServerboundUseItemPacket::kId:
        // 1.19 numbers each prediction so the server can confirm it; nothing below expects the answer.
        encode(packet, [&](io::ByteBuffer& output) {
            output.writeBytes(packet.body);
            protocol::varInt::write(output, 0);
        });
        break;
    default:
        break;
    }
    return true;
}

bool Link758::serverboundForeign(protocol::ProtocolState state, MutablePacket& packet) {
    return state == protocol::ProtocolState::Play && chatShapes::from1_18(packet);
}

} // namespace basalt::wire::multiversion::java
