#include <basalt/wire/protocol/ProtocolBuffer.hpp>
#include <basalt/wire/protocol/play/BorderPackets.hpp>

namespace basalt::wire::protocol::play {
namespace {

/// Every packet here is a fixed handful of scalars, so the only failure mode is a short frame.
template <typename Packet>
std::optional<Packet> finish(io::ByteBuffer& buffer, Packet&& packet) {
    return buffer.failed() ? std::nullopt : std::optional{std::forward<Packet>(packet)};
}

} // namespace

void InitializeBorderPacket::write(io::ByteBuffer& buffer) const {
    buffer.writeDouble(centerX);
    buffer.writeDouble(centerZ);
    buffer.writeDouble(oldSize);
    buffer.writeDouble(newSize);
    varLong::write(buffer, lerpTimeTicks);
    varInt::write(buffer, absoluteMaxSize);
    varInt::write(buffer, warningBlocks);
    varInt::write(buffer, warningSeconds);
}

std::optional<InitializeBorderPacket> InitializeBorderPacket::read(io::ByteBuffer& buffer) {
    InitializeBorderPacket packet;
    packet.centerX = buffer.readDouble();
    packet.centerZ = buffer.readDouble();
    packet.oldSize = buffer.readDouble();
    packet.newSize = buffer.readDouble();
    packet.lerpTimeTicks = varLong::read(buffer);
    packet.absoluteMaxSize = varInt::read(buffer);
    packet.warningBlocks = varInt::read(buffer);
    packet.warningSeconds = varInt::read(buffer);
    return finish(buffer, std::move(packet));
}

void SetBorderCenterPacket::write(io::ByteBuffer& buffer) const {
    buffer.writeDouble(centerX);
    buffer.writeDouble(centerZ);
}

std::optional<SetBorderCenterPacket> SetBorderCenterPacket::read(io::ByteBuffer& buffer) {
    SetBorderCenterPacket packet{buffer.readDouble(), buffer.readDouble()};
    return finish(buffer, std::move(packet));
}

void SetBorderSizePacket::write(io::ByteBuffer& buffer) const { buffer.writeDouble(size); }

std::optional<SetBorderSizePacket> SetBorderSizePacket::read(io::ByteBuffer& buffer) {
    SetBorderSizePacket packet{buffer.readDouble()};
    return finish(buffer, std::move(packet));
}

void SetBorderLerpSizePacket::write(io::ByteBuffer& buffer) const {
    buffer.writeDouble(oldSize);
    buffer.writeDouble(newSize);
    varLong::write(buffer, lerpTimeTicks);
}

std::optional<SetBorderLerpSizePacket> SetBorderLerpSizePacket::read(io::ByteBuffer& buffer) {
    SetBorderLerpSizePacket packet;
    packet.oldSize = buffer.readDouble();
    packet.newSize = buffer.readDouble();
    packet.lerpTimeTicks = varLong::read(buffer);
    return finish(buffer, std::move(packet));
}

void SetBorderWarningDelayPacket::write(io::ByteBuffer& buffer) const { varInt::write(buffer, warningSeconds); }

std::optional<SetBorderWarningDelayPacket> SetBorderWarningDelayPacket::read(io::ByteBuffer& buffer) {
    SetBorderWarningDelayPacket packet{varInt::read(buffer)};
    return finish(buffer, std::move(packet));
}

void SetBorderWarningDistancePacket::write(io::ByteBuffer& buffer) const { varInt::write(buffer, warningBlocks); }

std::optional<SetBorderWarningDistancePacket> SetBorderWarningDistancePacket::read(io::ByteBuffer& buffer) {
    SetBorderWarningDistancePacket packet{varInt::read(buffer)};
    return finish(buffer, std::move(packet));
}

void SetCameraPacket::write(io::ByteBuffer& buffer) const { varInt::write(buffer, cameraEntityId); }

std::optional<SetCameraPacket> SetCameraPacket::read(io::ByteBuffer& buffer) {
    SetCameraPacket packet{varInt::read(buffer)};
    return finish(buffer, std::move(packet));
}

void TickingStatePacket::write(io::ByteBuffer& buffer) const {
    buffer.writeFloat(tickRate);
    writeBoolean(buffer, frozen);
}

std::optional<TickingStatePacket> TickingStatePacket::read(io::ByteBuffer& buffer) {
    TickingStatePacket packet{buffer.readFloat(), readBoolean(buffer)};
    return finish(buffer, std::move(packet));
}

void TickingStepPacket::write(io::ByteBuffer& buffer) const { varInt::write(buffer, steps); }

std::optional<TickingStepPacket> TickingStepPacket::read(io::ByteBuffer& buffer) {
    TickingStepPacket packet{varInt::read(buffer)};
    return finish(buffer, std::move(packet));
}

void TransferPacket::write(io::ByteBuffer& buffer) const {
    writeString(buffer, host);
    varInt::write(buffer, port);
}

std::optional<TransferPacket> TransferPacket::read(io::ByteBuffer& buffer) {
    TransferPacket packet;
    packet.host = readString(buffer);
    packet.port = varInt::read(buffer);
    return finish(buffer, std::move(packet));
}

} // namespace basalt::wire::protocol::play
