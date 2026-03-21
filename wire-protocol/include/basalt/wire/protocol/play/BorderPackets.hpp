#pragma once

#include <basalt/wire/io/ByteBuffer.hpp>

#include <cstdint>
#include <optional>
#include <string>

namespace basalt::wire::protocol::play {

/// Clientbound, id 0x2c: the whole world border at once, which is what a joining client needs.
struct InitializeBorderPacket {
    static constexpr std::int32_t kId = 0x2c;
    /// parity: WorldBorder.ABSOLUTE_MAX_SIZE, which is what vanilla always sends here.
    static constexpr std::int32_t kAbsoluteMaxSize = 29999984;

    double centerX = 0.0;
    double centerZ = 0.0;
    double oldSize = 0.0;
    double newSize = 0.0;
    std::int64_t lerpTimeTicks = 0;
    std::int32_t absoluteMaxSize = kAbsoluteMaxSize;
    std::int32_t warningBlocks = 0;
    std::int32_t warningSeconds = 0;

    void write(io::ByteBuffer& buffer) const;
    [[nodiscard]] static std::optional<InitializeBorderPacket> read(io::ByteBuffer& buffer);

    friend bool operator==(const InitializeBorderPacket&, const InitializeBorderPacket&) = default;
};

/// Clientbound, id 0x5a: where the border is centred.
struct SetBorderCenterPacket {
    static constexpr std::int32_t kId = 0x5a;

    double centerX = 0.0;
    double centerZ = 0.0;

    void write(io::ByteBuffer& buffer) const;
    [[nodiscard]] static std::optional<SetBorderCenterPacket> read(io::ByteBuffer& buffer);

    friend bool operator==(const SetBorderCenterPacket&, const SetBorderCenterPacket&) = default;
};

/// Clientbound, id 0x5c: the border's size, changed at once.
struct SetBorderSizePacket {
    static constexpr std::int32_t kId = 0x5c;

    double size = 0.0;

    void write(io::ByteBuffer& buffer) const;
    [[nodiscard]] static std::optional<SetBorderSizePacket> read(io::ByteBuffer& buffer);

    friend bool operator==(const SetBorderSizePacket&, const SetBorderSizePacket&) = default;
};

/// Clientbound, id 0x5b: the border's size, moved to over a span of ticks.
struct SetBorderLerpSizePacket {
    static constexpr std::int32_t kId = 0x5b;

    double oldSize = 0.0;
    double newSize = 0.0;
    std::int64_t lerpTimeTicks = 0;

    void write(io::ByteBuffer& buffer) const;
    [[nodiscard]] static std::optional<SetBorderLerpSizePacket> read(io::ByteBuffer& buffer);

    friend bool operator==(const SetBorderLerpSizePacket&, const SetBorderLerpSizePacket&) = default;
};

/// Clientbound, id 0x5d: how many seconds before a shrinking border the warning appears.
struct SetBorderWarningDelayPacket {
    static constexpr std::int32_t kId = 0x5d;

    std::int32_t warningSeconds = 0;

    void write(io::ByteBuffer& buffer) const;
    [[nodiscard]] static std::optional<SetBorderWarningDelayPacket> read(io::ByteBuffer& buffer);

    friend bool operator==(const SetBorderWarningDelayPacket&, const SetBorderWarningDelayPacket&) = default;
};

/// Clientbound, id 0x5e: how close to the border the warning appears.
struct SetBorderWarningDistancePacket {
    static constexpr std::int32_t kId = 0x5e;

    std::int32_t warningBlocks = 0;

    void write(io::ByteBuffer& buffer) const;
    [[nodiscard]] static std::optional<SetBorderWarningDistancePacket> read(io::ByteBuffer& buffer);

    friend bool operator==(const SetBorderWarningDistancePacket&, const SetBorderWarningDistancePacket&) = default;
};

/// Clientbound, id 0x5f: whose eyes the client is looking through. Its own id puts it back.
struct SetCameraPacket {
    static constexpr std::int32_t kId = 0x5f;

    std::int32_t cameraEntityId = 0;

    void write(io::ByteBuffer& buffer) const;
    [[nodiscard]] static std::optional<SetCameraPacket> read(io::ByteBuffer& buffer);

    friend bool operator==(const SetCameraPacket&, const SetCameraPacket&) = default;
};

/// Clientbound, id 0x82: the tick rate the server is running at, and whether it is frozen.
struct TickingStatePacket {
    static constexpr std::int32_t kId = 0x82;

    float tickRate = 20.0F;
    bool frozen = false;

    void write(io::ByteBuffer& buffer) const;
    [[nodiscard]] static std::optional<TickingStatePacket> read(io::ByteBuffer& buffer);

    friend bool operator==(const TickingStatePacket&, const TickingStatePacket&) = default;
};

/// Clientbound, id 0x83: how many ticks a frozen server is about to step through.
struct TickingStepPacket {
    static constexpr std::int32_t kId = 0x83;

    std::int32_t steps = 0;

    void write(io::ByteBuffer& buffer) const;
    [[nodiscard]] static std::optional<TickingStepPacket> read(io::ByteBuffer& buffer);

    friend bool operator==(const TickingStepPacket&, const TickingStepPacket&) = default;
};

/// Clientbound, id 0x84: tells the client to reconnect somewhere else, keeping its cookies.
struct TransferPacket {
    static constexpr std::int32_t kId = 0x84;

    std::string host;
    std::int32_t port = 0;

    void write(io::ByteBuffer& buffer) const;
    [[nodiscard]] static std::optional<TransferPacket> read(io::ByteBuffer& buffer);

    friend bool operator==(const TransferPacket&, const TransferPacket&) = default;
};

} // namespace basalt::wire::protocol::play
