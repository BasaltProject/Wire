#include <basalt/wire/protocol/ProtocolBuffer.hpp>
#include <basalt/wire/protocol/play/VehiclePackets.hpp>

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

} // namespace basalt::wire::protocol::play
