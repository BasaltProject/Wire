#pragma once

#include <basalt/wire/io/ByteBuffer.hpp>

#include <cstdint>
#include <optional>
#include <string>

namespace basalt::wire::protocol::handshake {

/// parity: net.minecraft.network.protocol.handshake.ClientIntent.
enum class ClientIntent : std::int32_t {
    Status = 1,
    Login = 2,
    Transfer = 3,
};

[[nodiscard]] constexpr std::optional<ClientIntent> clientIntentFromId(std::int32_t id) noexcept {
    if (id >= 1 && id <= 3) {
        return static_cast<ClientIntent>(id);
    }
    return std::nullopt;
}

/// parity: ClientIntentionPacket, the only packet in the handshake state, always id 0x00.
struct HandshakePacket {
    static constexpr std::int32_t kId = 0;
    static constexpr std::int32_t kMaxHostLength = 255;
    /// What a host may run to under BungeeCord forwarding: a signed skin alone is over vanilla's 255.
    static constexpr std::int32_t kForwardedHostLength = 4096;

    std::int32_t protocolVersion = 0;
    std::string hostName;
    std::uint16_t port = 0;
    ClientIntent intent = ClientIntent::Status;

    void write(io::ByteBuffer& buffer) const;

    [[nodiscard]] static std::optional<HandshakePacket> read(io::ByteBuffer& buffer,
                                                             std::int32_t maxHostLength = kMaxHostLength);

    friend bool operator==(const HandshakePacket&, const HandshakePacket&) = default;
};

} // namespace basalt::wire::protocol::handshake
