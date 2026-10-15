#include <basalt/wire/auth/forwarding/VelocityForwarding.hpp>
#include <basalt/wire/io/ByteBuffer.hpp>
#include <basalt/wire/protocol/ProtocolBuffer.hpp>

#include <array>
#include <basalt_mbedtls_threads.h>
#include <cstdint>
#include <cstring>
#include <mbedtls/md.h>

namespace basalt::wire::auth::forwarding::velocity {
namespace {

/// The tag is a fixed-size SHA-256 HMAC, so the payload starts right after it.
constexpr std::size_t kSignatureLength = 32;
constexpr std::int32_t kMaxNameLength = 16;

std::array<std::uint8_t, kSignatureLength> hmac(std::string_view secret, std::span<const std::uint8_t> payload) {
    std::array<std::uint8_t, kSignatureLength> tag{};
    basalt_mbedtls_threading_init();
    const mbedtls_md_info_t* info = mbedtls_md_info_from_type(MBEDTLS_MD_SHA256);
    mbedtls_md_hmac(info, reinterpret_cast<const unsigned char*>(secret.data()), secret.size(), payload.data(),
                    payload.size(), tag.data());
    return tag;
}

/// Constant time, because a byte-by-byte compare on a signature leaks where it first differed and
/// that is enough to forge one a byte at a time.
bool sameTag(std::span<const std::uint8_t> a, std::span<const std::uint8_t> b) noexcept {
    if (a.size() != b.size()) {
        return false;
    }
    std::uint8_t difference = 0;
    for (std::size_t i = 0; i < a.size(); ++i) {
        difference |= static_cast<std::uint8_t>(a[i] ^ b[i]);
    }
    return difference == 0;
}

ForwardingResult readPayload(std::span<const std::uint8_t> payload) {
    io::ByteBuffer buffer = io::ByteBuffer::wrap(std::vector<std::uint8_t>{payload.begin(), payload.end()});

    const std::int32_t version = protocol::varInt::read(buffer);
    if (version > kMaxSupportedVersion) {
        return "the proxy forwards version " + std::to_string(version) + ", and this server reads up to " +
               std::to_string(kMaxSupportedVersion);
    }

    ForwardedLogin login;
    login.address = protocol::readString(buffer);
    login.profileId = protocol::readUuid(buffer);
    login.name = protocol::readString(buffer, kMaxNameLength);

    const std::int32_t count = protocol::readCollectionSize(buffer, 3);
    login.properties.reserve(static_cast<std::size_t>(count));
    for (std::int32_t i = 0; i < count && !buffer.failed(); ++i) {
        SessionProfileProperty property;
        property.name = protocol::readString(buffer);
        property.value = protocol::readString(buffer);
        if (protocol::readBoolean(buffer)) {
            property.signature = protocol::readString(buffer);
        }
        login.properties.push_back(std::move(property));
    }

    if (buffer.failed()) {
        return std::string{"the forwarded payload is malformed"};
    }
    // Whatever a version 2 or 3 proxy appended is the chat signing key, which nothing reads here.
    return login;
}

} // namespace

std::vector<std::uint8_t> request() { return {static_cast<std::uint8_t>(kMaxSupportedVersion)}; }

ForwardingResult decode(std::span<const std::uint8_t> data, const ProxyForwarding& forwarding) {
    if (const std::optional<std::string> problem = forwarding.misconfiguration()) {
        return *problem;
    }
    if (data.size() <= kSignatureLength) {
        return std::string{"the forwarded payload is too short"};
    }

    const std::span<const std::uint8_t> signature = data.first(kSignatureLength);
    const std::span<const std::uint8_t> payload = data.subspan(kSignatureLength);
    if (!sameTag(signature, hmac(forwarding.secret, payload))) {
        return std::string{"the forwarded payload is not signed with the configured secret"};
    }
    return readPayload(payload);
}

std::vector<std::uint8_t> sign(std::string_view secret, std::string_view address, const common::Uuid& profileId,
                               std::string_view name, const std::vector<SessionProfileProperty>& properties,
                               std::int32_t version) {
    io::ByteBuffer buffer;
    protocol::varInt::write(buffer, version);
    protocol::writeString(buffer, address);
    protocol::writeUuid(buffer, profileId);
    protocol::writeString(buffer, name);
    protocol::varInt::write(buffer, static_cast<std::int32_t>(properties.size()));
    for (const SessionProfileProperty& property : properties) {
        protocol::writeString(buffer, property.name);
        protocol::writeString(buffer, property.value);
        protocol::writeBoolean(buffer, property.signature.has_value());
        if (property.signature) {
            protocol::writeString(buffer, *property.signature);
        }
    }

    const std::vector<std::uint8_t> payload = buffer.toVector();
    const std::array<std::uint8_t, kSignatureLength> tag = hmac(secret, payload);

    std::vector<std::uint8_t> out;
    out.reserve(tag.size() + payload.size());
    out.insert(out.end(), tag.begin(), tag.end());
    out.insert(out.end(), payload.begin(), payload.end());
    return out;
}

} // namespace basalt::wire::auth::forwarding::velocity
