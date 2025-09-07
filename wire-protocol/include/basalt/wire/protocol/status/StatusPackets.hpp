#pragma once

#include <basalt/wire/io/ByteBuffer.hpp>
#include <basalt/wire/text/Component.hpp>

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace basalt::wire::protocol::status {

/// The JSON body of the Server List Ping response. parity: net.minecraft.network.protocol.status
/// .ServerStatus, every field lenient-optional except the description.
struct ServerStatusPayload {
    struct SamplePlayer {
        std::string name;
        std::string id;

        friend bool operator==(const SamplePlayer&, const SamplePlayer&) = default;
    };

    struct Players {
        std::int32_t max = 0;
        std::int32_t online = 0;
        std::vector<SamplePlayer> sample;

        friend bool operator==(const Players&, const Players&) = default;
    };

    struct Version {
        std::string name;
        std::int32_t protocol = 0;

        friend bool operator==(const Version&, const Version&) = default;
    };

    text::Component description;
    std::optional<Players> players;
    std::optional<Version> version;
    /// Raw PNG bytes; the wire form is a `data:image/png;base64,` URL.
    std::optional<std::vector<std::uint8_t>> favicon;
    bool enforcesSecureChat = false;

    [[nodiscard]] std::string toJsonString() const;

    [[nodiscard]] static std::optional<ServerStatusPayload> fromJsonString(std::string_view text);

    friend bool operator==(const ServerStatusPayload&, const ServerStatusPayload&) = default;
};

/// Serverbound, id 0x00, empty.
inline constexpr std::int32_t kStatusRequestId = 0;

/// Clientbound, id 0x00: the Server List Ping JSON body.
struct StatusResponsePacket {
    static constexpr std::int32_t kId = 0;

    ServerStatusPayload status;

    void write(io::ByteBuffer& buffer) const;
    [[nodiscard]] static std::optional<StatusResponsePacket> read(io::ByteBuffer& buffer);
};

/// Serverbound, id 0x01: an opaque timestamp to echo back. The clientbound pong shares the id and
/// the shape.
struct PingPacket {
    static constexpr std::int32_t kId = 1;

    std::int64_t time = 0;

    void write(io::ByteBuffer& buffer) const { buffer.writeLong(time); }

    [[nodiscard]] static std::optional<PingPacket> read(io::ByteBuffer& buffer) {
        PingPacket packet{buffer.readLong()};
        return buffer.failed() ? std::nullopt : std::optional{packet};
    }

    friend bool operator==(const PingPacket&, const PingPacket&) = default;
};

} // namespace basalt::wire::protocol::status
