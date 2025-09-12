#include <basalt/wire/protocol/ProtocolBuffer.hpp>
#include <basalt/wire/protocol/login/LoginPluginPackets.hpp>

#include <algorithm>

namespace basalt::wire::protocol::login {
namespace {

/// The payload has no length of its own: it is whatever is left in the frame, capped.
std::vector<std::uint8_t> readRemaining(io::ByteBuffer& buffer) {
    const auto available = static_cast<std::int32_t>(
        std::min<std::size_t>(buffer.readableBytes(), static_cast<std::size_t>(kMaxLoginPayloadSize)));
    return buffer.readBytes(static_cast<std::size_t>(available));
}

} // namespace

void LoginPluginRequestPacket::write(io::ByteBuffer& buffer) const {
    varInt::write(buffer, transactionId);
    writeString(buffer, channel);
    buffer.writeBytes(payload);
}

std::optional<LoginPluginRequestPacket> LoginPluginRequestPacket::read(io::ByteBuffer& buffer) {
    LoginPluginRequestPacket packet;
    packet.transactionId = varInt::read(buffer);
    packet.channel = readString(buffer);
    if (buffer.failed()) {
        return std::nullopt;
    }
    packet.payload = readRemaining(buffer);
    return buffer.failed() ? std::nullopt : std::optional{std::move(packet)};
}

void LoginPluginResponsePacket::write(io::ByteBuffer& buffer) const {
    varInt::write(buffer, transactionId);
    writeBoolean(buffer, payload.has_value());
    if (payload) {
        buffer.writeBytes(*payload);
    }
}

std::optional<LoginPluginResponsePacket> LoginPluginResponsePacket::read(io::ByteBuffer& buffer) {
    LoginPluginResponsePacket packet;
    packet.transactionId = varInt::read(buffer);
    const bool present = readBoolean(buffer);
    if (buffer.failed()) {
        return std::nullopt;
    }
    if (present) {
        packet.payload = readRemaining(buffer);
    }
    return buffer.failed() ? std::nullopt : std::optional{std::move(packet)};
}

} // namespace basalt::wire::protocol::login
