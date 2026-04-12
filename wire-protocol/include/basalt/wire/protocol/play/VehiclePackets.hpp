#pragma once

#include <basalt/wire/io/ByteBuffer.hpp>

#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

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

/// parity: `NewMinecartBehavior.MinecartStep`, one point a cart passes through, weighted by how far.
struct MinecartStep {
    std::array<double, 3> position{};
    std::array<double, 3> movement{};
    float yRot = 0.0F;
    float xRot = 0.0F;
    float weight = 0.0F;

    friend bool operator==(const MinecartStep&, const MinecartStep&) = default;
};

/// parity: `ClientboundMoveMinecartPacket`, a cart's turn as the steps it took, under the
/// `minecart_improvements` feature. The rotations travel as packed bytes.
struct ClientboundMoveMinecartPacket {
    static constexpr std::int32_t kId = 0x38;

    std::int32_t entityId = 0;
    std::vector<MinecartStep> steps;

    void write(io::ByteBuffer& buffer) const;
    [[nodiscard]] static std::optional<ClientboundMoveMinecartPacket> read(io::ByteBuffer& buffer);

    friend bool operator==(const ClientboundMoveMinecartPacket&, const ClientboundMoveMinecartPacket&) = default;
};

/// parity: `ServerboundSetCommandMinecartPacket`, what somebody typed into a command block cart.
struct ServerboundSetCommandMinecartPacket {
    static constexpr std::int32_t kId = 0x38;

    std::int32_t entityId = 0;
    std::string command;
    bool trackOutput = false;

    void write(io::ByteBuffer& buffer) const;
    [[nodiscard]] static std::optional<ServerboundSetCommandMinecartPacket> read(io::ByteBuffer& buffer);

    friend bool operator==(const ServerboundSetCommandMinecartPacket&,
                           const ServerboundSetCommandMinecartPacket&) = default;
};

} // namespace basalt::wire::protocol::play
