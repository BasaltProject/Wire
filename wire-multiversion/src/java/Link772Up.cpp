#include <basalt/wire/common/SafeInt.hpp>
#include <basalt/wire/multiversion/java/JavaTranslators.hpp>
#include <basalt/wire/multiversion/java/Link772Up.hpp>
#include <basalt/wire/multiversion/java/LoginFields.hpp>
#include <basalt/wire/multiversion/java/Tables772.hpp>
#include <basalt/wire/protocol/ProtocolBuffer.hpp>
#include <basalt/wire/protocol/VarInt.hpp>
#include <basalt/wire/protocol/configuration/ConfigurationPackets.hpp>
#include <basalt/wire/protocol/play/BorderPackets.hpp>
#include <basalt/wire/protocol/play/EntityData.hpp>
#include <basalt/wire/protocol/play/EntityPackets.hpp>
#include <basalt/wire/protocol/play/PlayPackets.hpp>
#include <basalt/wire/protocol/play/SimplePackets.hpp>

#include <string>

namespace basalt::wire::multiversion::java {
namespace {

namespace play = protocol::play;
using protocol::ProtocolState;

constexpr std::int32_t kAddEntity773 = 1;
constexpr std::int32_t kInitializeBorder773 = 42;
constexpr std::int32_t kLogin773 = 48;
constexpr std::int32_t kExplode773 = 36;
constexpr std::int32_t kPlayerRotation773 = 71;
constexpr std::int32_t kRespawn773 = 80;
constexpr std::int32_t kSetBorderCenter773 = 86;
constexpr std::int32_t kSpawnPosition773 = 95;
constexpr std::int32_t kSetEntityMotion773 = 99;

constexpr double kVelocityScale = 8000.0;

std::int32_t inStep(bool clientbound, ProtocolState state, std::int32_t id773) {
    const PacketIdTables& packets = tables772().packets;
    return clientbound ? packets.toOlder(state, id773) : packets.serverboundToOlder(state, id773);
}

float degrees(std::uint8_t angle) { return static_cast<float>(angle) * 360.0F / 256.0F; }

} // namespace

bool Link772Up::clientbound(ProtocolState state, MutablePacket& packet) {
    if (!RenumberingLink::clientbound(state, packet)) {
        return false;
    }
    static const std::int32_t registryData =
        inStep(true, ProtocolState::Configuration, protocol::configuration::RegistryDataPacket::kId);
    static const std::int32_t addEntity = inStep(true, ProtocolState::Play, kAddEntity773);
    static const std::int32_t initializeBorder = inStep(true, ProtocolState::Play, kInitializeBorder773);
    static const std::int32_t login = inStep(true, ProtocolState::Play, kLogin773);
    static const std::int32_t explode = inStep(true, ProtocolState::Play, kExplode773);
    static const std::int32_t playerRotation = inStep(true, ProtocolState::Play, kPlayerRotation773);
    static const std::int32_t respawn = inStep(true, ProtocolState::Play, kRespawn773);
    static const std::int32_t borderCenter = inStep(true, ProtocolState::Play, kSetBorderCenter773);
    static const std::int32_t spawnPosition = inStep(true, ProtocolState::Play, kSpawnPosition773);
    static const std::int32_t motion = inStep(true, ProtocolState::Play, kSetEntityMotion773);

    if (state == ProtocolState::Configuration && packet.id == registryData) {
        const auto data =
            decode(packet, [](io::ByteBuffer& in) { return protocol::configuration::RegistryDataPacket::read(in); });
        if (data.has_value() && data->registryId == "minecraft:dimension_type") {
            scales_.clear();
            for (const protocol::configuration::RegistryEntry& entry : data->entries) {
                const nbt::NbtCompound* element = entry.value ? entry.value->get<nbt::NbtCompound>() : nullptr;
                scales_.push_back(element == nullptr ? 1.0F : element->getFloat("coordinate_scale", 1.0F));
            }
        }
        return true;
    }
    if (state != ProtocolState::Play) {
        return true;
    }
    const std::int32_t id = packet.id;
    if (id == login || id == respawn) {
        io::ByteBuffer input = io::ByteBuffer::wrap(packet.body);
        LoginFields::SpawnFields spawn;
        if (id == login) {
            spawn = LoginFields::read(input, false, true, false).spawn;
        } else {
            spawn = LoginFields::SpawnFields::read(input, true, false);
        }
        if (!input.failed()) {
            dimension_ = spawn.dimensionName;
            const auto index = static_cast<std::size_t>(std::max(spawn.dimension, 0));
            scale_ = index < scales_.size() ? scales_[index] : 1.0F;
        }
    } else if (id == motion) {
        io::ByteBuffer input = io::ByteBuffer::wrap(packet.body);
        const std::int32_t entityId = protocol::varInt::read(input);
        const double x = input.readShort() / kVelocityScale;
        const double y = input.readShort() / kVelocityScale;
        const double z = input.readShort() / kVelocityScale;
        if (!input.failed()) {
            encode(packet, [&](io::ByteBuffer& out) {
                play::SetEntityMotionPacket{.entityId = entityId, .velocityX = x, .velocityY = y, .velocityZ = z}.write(
                    out);
            });
        }
    } else if (id == addEntity) {
        io::ByteBuffer input = io::ByteBuffer::wrap(packet.body);
        play::SpawnEntityPacket spawn;
        spawn.entityId = protocol::varInt::read(input);
        spawn.uuid = protocol::readUuid(input);
        spawn.entityType = protocol::varInt::read(input);
        spawn.x = input.readDouble();
        spawn.y = input.readDouble();
        spawn.z = input.readDouble();
        spawn.pitch = degrees(input.readByte());
        spawn.yaw = degrees(input.readByte());
        spawn.headYaw = degrees(input.readByte());
        spawn.objectData = protocol::varInt::read(input);
        spawn.velocityX = input.readShort() / kVelocityScale;
        spawn.velocityY = input.readShort() / kVelocityScale;
        spawn.velocityZ = input.readShort() / kVelocityScale;
        if (!input.failed()) {
            encode(packet, [&](io::ByteBuffer& out) { spawn.write(out); });
        }
    } else if (id == explode) {
        io::ByteBuffer input = io::ByteBuffer::wrap(packet.body);
        play::ExplodePacket explosion;
        explosion.x = input.readDouble();
        explosion.y = input.readDouble();
        explosion.z = input.readDouble();
        if (input.readByte() != 0) {
            explosion.knockbackX = input.readDouble();
            explosion.knockbackY = input.readDouble();
            explosion.knockbackZ = input.readDouble();
        }
        explosion.particleId = protocol::varInt::read(input);
        static_cast<void>(protocol::varInt::read(input));
        explosion.sound = protocol::readString(input);
        static_cast<void>(input.readByte());
        if (!input.failed()) {
            encode(packet, [&](io::ByteBuffer& out) { explosion.write(out); });
        }
    } else if (id == spawnPosition) {
        io::ByteBuffer input = io::ByteBuffer::wrap(packet.body);
        const std::int64_t packed = input.readLong();
        const float yaw = input.readFloat();
        if (!input.failed()) {
            play::SpawnPositionPacket upgraded;
            upgraded.globalPos.dimensionName = dimension_;
            upgraded.globalPos.location = play::BlockPos::unpack(packed);
            upgraded.yaw = yaw;
            encode(packet, [&](io::ByteBuffer& out) { upgraded.write(out); });
        }
    } else if (id == playerRotation) {
        rewrite(packet, [](io::ByteBuffer& input, io::ByteBuffer& output) {
            output.writeFloat(input.readFloat());
            protocol::writeBoolean(output, false);
            output.writeFloat(input.readFloat());
            protocol::writeBoolean(output, false);
        });
    } else if (id == initializeBorder || id == borderCenter) {
        border(packet, id == initializeBorder);
    }
    return true;
}

void Link772Up::border(MutablePacket& packet, bool initialize) const {
    if (scale_ == 0.0F) {
        return;
    }
    io::ByteBuffer input = io::ByteBuffer::wrap(packet.body);
    const double divisor = static_cast<double>(scale_);
    if (initialize) {
        auto border = play::InitializeBorderPacket::read(input);
        if (border.has_value()) {
            border->centerX /= divisor;
            border->centerZ /= divisor;
            encode(packet, [&](io::ByteBuffer& out) { border->write(out); });
        }
    } else {
        auto center = play::SetBorderCenterPacket::read(input);
        if (center.has_value()) {
            center->centerX /= divisor;
            center->centerZ /= divisor;
            encode(packet, [&](io::ByteBuffer& out) { center->write(out); });
        }
    }
}

} // namespace basalt::wire::multiversion::java
