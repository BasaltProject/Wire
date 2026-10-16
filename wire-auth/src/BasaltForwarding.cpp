#include <basalt/wire/auth/SessionKeyPair.hpp>
#include <basalt/wire/auth/forwarding/BasaltForwarding.hpp>
#include <basalt/wire/io/ByteBuffer.hpp>
#include <basalt/wire/protocol/ProtocolBuffer.hpp>

#include <algorithm>
#include <cstring>
#include <mbedtls/sha256.h>
#include <monocypher-ed25519.h>

namespace basalt::wire::auth::forwarding::native {
namespace {

constexpr std::int32_t kMaxNameLength = 16;
constexpr std::int32_t kMaxKeyIdLength = 16;
constexpr std::size_t kMaxIdLength = 64;
constexpr std::size_t kMaxAddressLength = 64;
constexpr std::size_t kMaxHostLength = 255;
constexpr std::size_t kMaxProperties = 64;
constexpr std::size_t kMaxPropertyName = 256;
constexpr std::size_t kMaxPropertyValue = 32768;
constexpr std::size_t kMaxPropertySignature = 8192;
constexpr std::size_t kMaxStrings = 65536;
constexpr std::uint8_t kMaxOrigin = static_cast<std::uint8_t>(Origin::Other);

bool withinLimits(const Identity& identity) {
    if (identity.name.empty() || identity.name.size() > static_cast<std::size_t>(kMaxNameLength) ||
        identity.properties.size() > kMaxProperties || identity.address.empty() ||
        identity.address.size() > kMaxAddressLength || identity.virtualHost.size() > kMaxHostLength ||
        identity.proxyId.empty() || identity.proxyId.size() > kMaxIdLength || identity.originId.size() > kMaxIdLength ||
        static_cast<std::uint8_t>(identity.origin) > kMaxOrigin)
        return false;
    std::size_t strings = identity.name.size() + identity.address.size() + identity.virtualHost.size();
    for (const SessionProfileProperty& property : identity.properties) {
        if (property.name.size() > kMaxPropertyName || property.value.size() > kMaxPropertyValue ||
            (property.signature && property.signature->size() > kMaxPropertySignature))
            return false;
        strings += property.name.size() + property.value.size() + (property.signature ? property.signature->size() : 0);
    }
    return strings <= kMaxStrings;
}

std::vector<std::uint8_t> signedMessage(const Request& request, std::span<const std::uint8_t> body) {
    std::vector<std::uint8_t> message;
    message.reserve(kSigningDomain.size() + 1 + kChallengeSize + request.backendId.size() + 1 + body.size());
    message.insert(message.end(), kSigningDomain.begin(), kSigningDomain.end());
    message.push_back(0);
    message.insert(message.end(), request.challenge.begin(), request.challenge.end());
    message.insert(message.end(), request.backendId.begin(), request.backendId.end());
    message.push_back(0);
    message.insert(message.end(), body.begin(), body.end());
    return message;
}

void writeBody(io::ByteBuffer& out, std::uint8_t version, const std::string& id, const Identity& identity) {
    protocol::varInt::write(out, version);
    protocol::writeString(out, id);
    protocol::writeUuid(out, identity.profileId);
    protocol::writeString(out, identity.name);
    protocol::varInt::write(out, static_cast<std::int32_t>(identity.properties.size()));
    for (const SessionProfileProperty& property : identity.properties) {
        protocol::writeString(out, property.name);
        protocol::writeString(out, property.value);
        protocol::writeBoolean(out, property.signature.has_value());
        if (property.signature)
            protocol::writeString(out, *property.signature);
    }
    protocol::writeString(out, identity.address);
    out.writeShort(static_cast<std::int16_t>(identity.port));
    protocol::writeString(out, identity.virtualHost);
    out.writeShort(static_cast<std::int16_t>(identity.virtualPort));
    protocol::writeBoolean(out, identity.onlineMode);
    protocol::writeString(out, identity.proxyId);
    protocol::varInt::write(out, static_cast<std::int32_t>(identity.origin));
    protocol::writeString(out, identity.originId);
    protocol::varInt::write(out, static_cast<std::int32_t>(identity.capabilities));
    protocol::varInt::write(out, 0);
}

bool readIdentity(io::ByteBuffer& in, Identity& identity) {
    identity.profileId = protocol::readUuid(in);
    identity.name = protocol::readString(in, kMaxNameLength);
    const std::int32_t count = protocol::readCollectionSize(in, 3);
    if (count < 0 || static_cast<std::size_t>(count) > kMaxProperties)
        return false;
    identity.properties.reserve(static_cast<std::size_t>(count));
    for (std::int32_t i = 0; i < count && !in.failed(); ++i) {
        SessionProfileProperty property;
        property.name = protocol::readString(in, kMaxPropertyName);
        property.value = protocol::readString(in, kMaxPropertyValue);
        if (protocol::readBoolean(in))
            property.signature = protocol::readString(in, kMaxPropertySignature);
        identity.properties.push_back(std::move(property));
    }
    identity.address = protocol::readString(in, kMaxAddressLength);
    identity.port = static_cast<std::uint16_t>(in.readShort());
    identity.virtualHost = protocol::readString(in, kMaxHostLength);
    identity.virtualPort = static_cast<std::uint16_t>(in.readShort());
    identity.onlineMode = protocol::readBoolean(in);
    identity.proxyId = protocol::readString(in, kMaxIdLength);
    const std::int32_t origin = protocol::varInt::read(in);
    identity.originId = protocol::readString(in, kMaxIdLength);
    identity.capabilities = static_cast<std::uint32_t>(protocol::varInt::read(in));
    const std::int32_t extras = protocol::varInt::read(in);
    if (in.failed() || origin < 0 || origin > kMaxOrigin || extras != 0 || in.readableBytes() != 0)
        return false;
    identity.origin = static_cast<Origin>(origin);
    return true;
}

} // namespace

std::string keyId(const PublicKey& key) {
    std::array<std::uint8_t, 32> digest{};
    if (mbedtls_sha256(key.data(), key.size(), digest.data(), 0) != 0)
        return {};
    constexpr std::string_view kHex = "0123456789abcdef";
    std::string id;
    id.reserve(16);
    for (std::size_t i = 0; i < 8; ++i) {
        id.push_back(kHex[digest[i] >> 4]);
        id.push_back(kHex[digest[i] & 0xF]);
    }
    return id;
}

SigningKey::~SigningKey() { crypto_wipe(secret_.data(), secret_.size()); }

SigningKey SigningKey::fromSeed(std::span<const std::uint8_t, 32> seed) {
    SigningKey key;
    std::array<std::uint8_t, 32> scratch{};
    std::copy(seed.begin(), seed.end(), scratch.begin());
    crypto_ed25519_key_pair(key.secret_.data(), key.public_.data(), scratch.data());
    return key;
}

std::optional<SigningKey> SigningKey::generate() {
    std::array<std::uint8_t, 32> seed{};
    if (!randomBytes(seed))
        return std::nullopt;
    SigningKey key = fromSeed(seed);
    crypto_wipe(seed.data(), seed.size());
    return key;
}

std::span<const std::uint8_t, 32> SigningKey::seed() const noexcept {
    return std::span<const std::uint8_t, 32>{secret_.data(), 32};
}

std::array<std::uint8_t, kSignatureSize> SigningKey::signature(std::span<const std::uint8_t> message) const {
    std::array<std::uint8_t, kSignatureSize> out{};
    crypto_ed25519_sign(out.data(), secret_.data(), message.data(), message.size());
    return out;
}

std::optional<Request> newRequest(std::string_view backendId) {
    if (backendId.empty() || backendId.size() > kMaxIdLength)
        return std::nullopt;
    Request request;
    request.backendId = std::string{backendId};
    if (!randomBytes(request.challenge))
        return std::nullopt;
    return request;
}

std::vector<std::uint8_t> encodeRequest(const Request& request) {
    io::ByteBuffer out;
    out.writeByte(request.versionMin);
    out.writeByte(request.versionMax);
    out.writeBytes(request.challenge);
    protocol::writeString(out, request.backendId);
    protocol::varInt::write(out, static_cast<std::int32_t>(request.capabilities));
    return out.toVector();
}

std::optional<Request> decodeRequest(std::span<const std::uint8_t> data) {
    io::ByteBuffer in = io::ByteBuffer::wrap(std::vector<std::uint8_t>{data.begin(), data.end()});
    Request request;
    request.versionMin = in.readByte();
    request.versionMax = in.readByte();
    const std::span<const std::uint8_t> challenge = in.readSpan(kChallengeSize);
    request.backendId = protocol::readString(in, static_cast<std::int32_t>(kMaxIdLength));
    request.capabilities = static_cast<std::uint32_t>(protocol::varInt::read(in));
    if (in.failed() || in.readableBytes() != 0 || request.versionMin > request.versionMax ||
        request.backendId.empty())
        return std::nullopt;
    request.capabilities &= kKnownCapabilities;
    std::copy(challenge.begin(), challenge.end(), request.challenge.begin());
    return request;
}

std::vector<std::uint8_t> sign(const SigningKey& key, const Request& request, std::uint8_t version,
                               const Identity& identity) {
    if (version < request.versionMin || version > request.versionMax || version != kVersion1 ||
        request.backendId.empty() || !withinLimits(identity) ||
        (identity.capabilities & ~request.capabilities) != 0)
        return {};
    const std::string id = keyId(key.publicKey());
    if (id.empty())
        return {};

    io::ByteBuffer body;
    writeBody(body, version, id, identity);
    const std::vector<std::uint8_t> bodyBytes = body.toVector();
    const auto signature = key.signature(signedMessage(request, bodyBytes));

    std::vector<std::uint8_t> response(signature.begin(), signature.end());
    response.insert(response.end(), bodyBytes.begin(), bodyBytes.end());
    return response;
}

VerifyResult verify(std::span<const std::uint8_t> response, const Request& issued, std::span<const PublicKey> trusted) {
    if (response.size() <= kSignatureSize || response.size() > kMaxResponseSize)
        return std::string{"the forwarding response has an invalid size"};
    const std::span<const std::uint8_t> body = response.subspan(kSignatureSize);

    io::ByteBuffer in = io::ByteBuffer::wrap(std::vector<std::uint8_t>{body.begin(), body.end()});
    const std::int32_t version = protocol::varInt::read(in);
    const std::string id = protocol::readString(in, kMaxKeyIdLength);
    if (in.failed())
        return std::string{"the forwarding response is malformed"};
    if (version < issued.versionMin || version > issued.versionMax || version != kVersion1)
        return "the proxy chose version " + std::to_string(version) + ", outside the range " +
               std::to_string(issued.versionMin) + " to " + std::to_string(issued.versionMax) + " offered";

    const auto key = std::ranges::find_if(trusted, [&](const PublicKey& candidate) { return keyId(candidate) == id; });
    if (key == trusted.end())
        return "the response is signed with key " + id + ", which is not trusted";

    const std::vector<std::uint8_t> message = signedMessage(issued, body);
    if (crypto_ed25519_check(response.data(), key->data(), message.data(), message.size()) != 0)
        return std::string{"the forwarding signature does not validate"};

    Verified verified;
    verified.version = static_cast<std::uint8_t>(version);
    verified.keyId = id;
    if (!readIdentity(in, verified.identity))
        return std::string{"the forwarded identity is malformed"};
    if ((verified.identity.capabilities & ~issued.capabilities) != 0 ||
        (verified.identity.capabilities & ~kKnownCapabilities) != 0)
        return std::string{"the proxy selected capabilities that were not offered"};
    return verified;
}

} // namespace basalt::wire::auth::forwarding::native
