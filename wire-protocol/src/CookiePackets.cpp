#include <basalt/wire/common/Identifier.hpp>
#include <basalt/wire/protocol/ProtocolBuffer.hpp>
#include <basalt/wire/protocol/cookie/CookiePackets.hpp>

#include <algorithm>

namespace basalt::wire::protocol::cookie {
namespace {
bool valid(std::string_view key) {
    auto parsed = common::Identifier::parse(key);
    if (!parsed)
        return false;
    const auto character = [](char c) {
        return (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_' || c == '-' || c == '.';
    };
    return std::ranges::all_of(parsed->namespaceName(), character) &&
           std::ranges::all_of(parsed->path(), [&](char c) { return character(c) || c == '/'; });
}

std::string readKey(io::ByteBuffer& buffer) {
    auto key = readString(buffer);
    if (!valid(key))
        buffer.markFailed();
    else
        key = common::Identifier::parse(key)->toString();
    return key;
}

void writeKey(io::ByteBuffer& buffer, std::string_view key) {
    if (!valid(key)) {
        buffer.markFailed();
        return;
    }
    writeString(buffer, common::Identifier::parse(key)->toString());
}
} // namespace

void RequestPacket::write(io::ByteBuffer& buffer) const { writeKey(buffer, key); }

std::optional<RequestPacket> RequestPacket::read(io::ByteBuffer& buffer) {
    RequestPacket packet{readKey(buffer)};
    return buffer.failed() ? std::nullopt : std::optional{std::move(packet)};
}

void ResponsePacket::write(io::ByteBuffer& buffer) const {
    if (payload && payload->size() > kMaxPayloadBytes) {
        buffer.markFailed();
        return;
    }
    writeKey(buffer, key);
    writeBoolean(buffer, payload.has_value());
    if (payload)
        writeByteArray(buffer, *payload);
}

std::optional<ResponsePacket> ResponsePacket::read(io::ByteBuffer& buffer) {
    ResponsePacket packet{readKey(buffer), std::nullopt};
    if (readBoolean(buffer))
        packet.payload = readByteArray(buffer, kMaxPayloadBytes);
    return buffer.failed() ? std::nullopt : std::optional{std::move(packet)};
}

void StorePacket::write(io::ByteBuffer& buffer) const {
    if (payload.size() > kMaxPayloadBytes) {
        buffer.markFailed();
        return;
    }
    writeKey(buffer, key);
    writeByteArray(buffer, payload);
}

std::optional<StorePacket> StorePacket::read(io::ByteBuffer& buffer) {
    StorePacket packet{readKey(buffer), readByteArray(buffer, kMaxPayloadBytes)};
    return buffer.failed() ? std::nullopt : std::optional{std::move(packet)};
}
} // namespace basalt::wire::protocol::cookie
