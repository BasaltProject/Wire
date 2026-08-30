#pragma once

#include <basalt/wire/io/ByteBuffer.hpp>

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace basalt::wire::protocol::login {

/// parity: ClientboundCustomQueryPacket.MAX_PAYLOAD_SIZE, the most a login query may carry either
/// way.
inline constexpr std::int32_t kMaxLoginPayloadSize = 1048576;

/// Clientbound, id 0x04: an out-of-band question during login. The payload is written raw and runs
/// to the end of the frame.
struct LoginPluginRequestPacket {
    static constexpr std::int32_t kId = 4;

    std::int32_t transactionId = 0;
    std::string channel;
    std::vector<std::uint8_t> payload;

    void write(io::ByteBuffer& buffer) const;
    [[nodiscard]] static std::optional<LoginPluginRequestPacket> read(io::ByteBuffer& buffer);

    friend bool operator==(const LoginPluginRequestPacket&, const LoginPluginRequestPacket&) = default;
};

/// Serverbound, id 0x02: the answer, matched by transaction id. An absent payload is "I do not know
/// that channel", which is what every real client answers.
struct LoginPluginResponsePacket {
    static constexpr std::int32_t kId = 2;

    std::int32_t transactionId = 0;
    std::optional<std::vector<std::uint8_t>> payload;

    void write(io::ByteBuffer& buffer) const;
    [[nodiscard]] static std::optional<LoginPluginResponsePacket> read(io::ByteBuffer& buffer);

    friend bool operator==(const LoginPluginResponsePacket&, const LoginPluginResponsePacket&) = default;
};

} // namespace basalt::wire::protocol::login
