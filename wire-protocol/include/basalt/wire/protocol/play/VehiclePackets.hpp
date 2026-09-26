#pragma once

#include <basalt/wire/io/ByteBuffer.hpp>

#include <cstdint>
#include <optional>

namespace basalt::wire::protocol::play {

/// Where a vehicle is and which way it faces. parity: `PositionAndRotation`.
struct VehiclePosition {
    double x = 0.0;
    double y = 0.0;
    double z = 0.0;
    float yRot = 0.0F;
    float xRot = 0.0F;

    friend bool operator==(const VehiclePosition&, const VehiclePosition&) = default;
};

/// parity: `ServerboundMoveVehiclePacket`, the vehicle its driver moves on their own client.
struct ServerboundMoveVehiclePacket {
    static constexpr std::int32_t kId = 0x22;

    VehiclePosition movingTo;
    bool onGround = false;

    void write(io::ByteBuffer& buffer) const;
    [[nodiscard]] static std::optional<ServerboundMoveVehiclePacket> read(io::ByteBuffer& buffer);

    friend bool operator==(const ServerboundMoveVehiclePacket&, const ServerboundMoveVehiclePacket&) = default;
};

/// parity: `ServerboundPaddleBoatPacket`, which oars the driver is pulling.
struct ServerboundPaddleBoatPacket {
    static constexpr std::int32_t kId = 0x23;

    bool left = false;
    bool right = false;

    void write(io::ByteBuffer& buffer) const;
    [[nodiscard]] static std::optional<ServerboundPaddleBoatPacket> read(io::ByteBuffer& buffer);

    friend bool operator==(const ServerboundPaddleBoatPacket&, const ServerboundPaddleBoatPacket&) = default;
};

/// parity: `ClientboundMoveVehiclePacket`, the server putting a driver's vehicle back.
struct ClientboundMoveVehiclePacket {
    static constexpr std::int32_t kId = 0x3a;

    VehiclePosition movingTo;

    void write(io::ByteBuffer& buffer) const;
    [[nodiscard]] static std::optional<ClientboundMoveVehiclePacket> read(io::ByteBuffer& buffer);

    friend bool operator==(const ClientboundMoveVehiclePacket&, const ClientboundMoveVehiclePacket&) = default;
};

/// parity: `ClientboundMountScreenOpenPacket`, the inventory of a horse, a llama or a camel.
struct MountScreenOpenPacket {
    static constexpr std::int32_t kId = 0x2a;

    std::int32_t containerId = 0;
    std::int32_t inventoryColumns = 0;
    std::int32_t entityId = 0;

    void write(io::ByteBuffer& buffer) const;
    [[nodiscard]] static std::optional<MountScreenOpenPacket> read(io::ByteBuffer& buffer);

    friend bool operator==(const MountScreenOpenPacket&, const MountScreenOpenPacket&) = default;
};

} // namespace basalt::wire::protocol::play
