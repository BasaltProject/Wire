#include <basalt/wire/protocol/ProtocolBuffer.hpp>
#include <basalt/wire/protocol/play/VehiclePackets.hpp>

#include <cmath>

namespace basalt::wire::protocol::play {
namespace {

void writePosition(io::ByteBuffer& buffer, const VehiclePosition& position) {
    buffer.writeDouble(position.x);
    buffer.writeDouble(position.y);
    buffer.writeDouble(position.z);
    buffer.writeFloat(position.yRot);
    buffer.writeFloat(position.xRot);
}

[[nodiscard]] VehiclePosition readPosition(io::ByteBuffer& buffer) {
    VehiclePosition position;
    position.x = buffer.readDouble();
    position.y = buffer.readDouble();
    position.z = buffer.readDouble();
    position.yRot = buffer.readFloat();
    position.xRot = buffer.readFloat();
    return position;
}

/// parity: `ByteBufCodecs.ROTATION_BYTE`.
void writeRotation(io::ByteBuffer& buffer, float degrees) {
    buffer.writeByte(static_cast<std::uint8_t>(
        static_cast<std::int8_t>(static_cast<std::int32_t>(std::floor(degrees * 256.0F / 360.0F)))));
}

[[nodiscard]] float readRotation(io::ByteBuffer& buffer) {
    return static_cast<float>(static_cast<std::int8_t>(buffer.readByte())) * 360.0F / 256.0F;
}

} // namespace

void ServerboundMoveVehiclePacket::write(io::ByteBuffer& buffer) const {
    writePosition(buffer, movingTo);
    buffer.writeByte(onGround ? 1 : 0);
}

std::optional<ServerboundMoveVehiclePacket> ServerboundMoveVehiclePacket::read(io::ByteBuffer& buffer) {
    ServerboundMoveVehiclePacket packet;
    packet.movingTo = readPosition(buffer);
    packet.onGround = buffer.readByte() != 0;
    return buffer.failed() ? std::nullopt : std::optional{packet};
}

void ServerboundPaddleBoatPacket::write(io::ByteBuffer& buffer) const {
    buffer.writeByte(left ? 1 : 0);
    buffer.writeByte(right ? 1 : 0);
}

std::optional<ServerboundPaddleBoatPacket> ServerboundPaddleBoatPacket::read(io::ByteBuffer& buffer) {
    ServerboundPaddleBoatPacket packet;
    packet.left = buffer.readByte() != 0;
    packet.right = buffer.readByte() != 0;
    return buffer.failed() ? std::nullopt : std::optional{packet};
}

void ClientboundMoveVehiclePacket::write(io::ByteBuffer& buffer) const { writePosition(buffer, movingTo); }

std::optional<ClientboundMoveVehiclePacket> ClientboundMoveVehiclePacket::read(io::ByteBuffer& buffer) {
    ClientboundMoveVehiclePacket packet;
    packet.movingTo = readPosition(buffer);
    return buffer.failed() ? std::nullopt : std::optional{packet};
}

void MountScreenOpenPacket::write(io::ByteBuffer& buffer) const {
    varInt::write(buffer, containerId);
    varInt::write(buffer, inventoryColumns);
    buffer.writeInt(entityId);
}

std::optional<MountScreenOpenPacket> MountScreenOpenPacket::read(io::ByteBuffer& buffer) {
    MountScreenOpenPacket packet;
    packet.containerId = varInt::read(buffer);
    packet.inventoryColumns = varInt::read(buffer);
    packet.entityId = buffer.readInt();
    return buffer.failed() ? std::nullopt : std::optional{packet};
}

void ClientboundMoveMinecartPacket::write(io::ByteBuffer& buffer) const {
    varInt::write(buffer, entityId);
    varInt::write(buffer, static_cast<std::int32_t>(steps.size()));
    for (const MinecartStep& step : steps) {
        for (const double value : step.position) {
            buffer.writeDouble(value);
        }
        for (const double value : step.movement) {
            buffer.writeDouble(value);
        }
        writeRotation(buffer, step.yRot);
        writeRotation(buffer, step.xRot);
        buffer.writeFloat(step.weight);
    }
}

std::optional<ClientboundMoveMinecartPacket> ClientboundMoveMinecartPacket::read(io::ByteBuffer& buffer) {
    ClientboundMoveMinecartPacket packet;
    packet.entityId = varInt::read(buffer);
    const std::int32_t count = varInt::read(buffer);
    for (std::int32_t i = 0; i < count && !buffer.failed(); ++i) {
        MinecartStep step;
        for (double& value : step.position) {
            value = buffer.readDouble();
        }
        for (double& value : step.movement) {
            value = buffer.readDouble();
        }
        step.yRot = readRotation(buffer);
        step.xRot = readRotation(buffer);
        step.weight = buffer.readFloat();
        packet.steps.push_back(step);
    }
    return buffer.failed() ? std::nullopt : std::optional{std::move(packet)};
}

void ServerboundSetCommandMinecartPacket::write(io::ByteBuffer& buffer) const {
    varInt::write(buffer, entityId);
    writeString(buffer, command);
    buffer.writeByte(trackOutput ? 1 : 0);
}

std::optional<ServerboundSetCommandMinecartPacket> ServerboundSetCommandMinecartPacket::read(io::ByteBuffer& buffer) {
    ServerboundSetCommandMinecartPacket packet;
    packet.entityId = varInt::read(buffer);
    packet.command = readString(buffer);
    packet.trackOutput = buffer.readByte() != 0;
    return buffer.failed() ? std::nullopt : std::optional{std::move(packet)};
}

} // namespace basalt::wire::protocol::play
