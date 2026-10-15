#include <basalt/wire/auth/forwarding/VelocityForwarding.hpp>
#include <basalt/wire/io/ByteBuffer.hpp>
#include <basalt/wire/protocol/ProtocolBuffer.hpp>

#include <algorithm>
#include <array>
#include <basalt_mbedtls_threads.h>
#include <cstdint>
#include <cstring>
#include <mbedtls/md.h>

namespace basalt::wire::auth::forwarding::velocity {
namespace {

constexpr std::size_t kSignatureLength = 32;
constexpr std::int32_t kMaxNameLength = 16;

std::optional<std::array<std::uint8_t, kSignatureLength>> hmac(std::string_view secret,
                                                               std::span<const std::uint8_t> payload) {
    std::array<std::uint8_t, kSignatureLength> tag{};
    basalt_mbedtls_threading_init();
    const mbedtls_md_info_t* info = mbedtls_md_info_from_type(MBEDTLS_MD_SHA256);
    if (!info || mbedtls_md_hmac(info, reinterpret_cast<const unsigned char*>(secret.data()), secret.size(),
                                 payload.data(), payload.size(), tag.data()) != 0)
        return std::nullopt;
    return tag;
}

/// Constant time: a byte-by-byte compare leaks where a signature first differed.
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
    const auto tag = hmac(forwarding.secret, payload);
    if (!tag || !sameTag(signature, *tag)) {
        return std::string{"the forwarded payload is not signed with the configured secret"};
    }
    return readPayload(payload);
}

std::vector<std::uint8_t> sign(std::string_view secret, std::string_view address, const common::Uuid& profileId,
                               std::string_view name, const std::vector<SessionProfileProperty>& properties,
                               std::int32_t version) {
    io::ByteBuffer buffer;
    if (!signInto(buffer, secret, address, profileId, name, properties, version))
        return {};
    return buffer.toVector();
}

std::expected<void, EncodeError> signInto(io::ByteBuffer& output, std::string_view secret, std::string_view address,
                                          const common::Uuid& profileId, std::string_view name,
                                          const std::vector<SessionProfileProperty>& properties, std::int32_t version) {
    if (version != kVersionDefault && version != kVersionLazySession)
        return std::unexpected(EncodeError::kUnsupportedVersion);
    if (secret.empty() || address.empty() || address.size() > 64 || name.empty() || name.size() > 16 ||
        properties.size() > 64)
        return std::unexpected(EncodeError::kInvalidIdentity);
    std::size_t strings = address.size() + name.size();
    for (const auto& property : properties) {
        if (property.name.size() > 256 || property.value.size() > 32768 ||
            (property.signature && property.signature->size() > 8192))
            return std::unexpected(EncodeError::kInvalidIdentity);
        strings += property.name.size() + property.value.size() + (property.signature ? property.signature->size() : 0);
    }
    if (strings > 65536)
        return std::unexpected(EncodeError::kInvalidIdentity);
    const auto offset = output.writerIndex();
    const std::array<std::uint8_t, kSignatureLength> blank{};
    output.writeBytes(blank);
    protocol::varInt::write(output, version);
    protocol::writeString(output, address);
    protocol::writeUuid(output, profileId);
    protocol::writeString(output, name);
    protocol::varInt::write(output, static_cast<std::int32_t>(properties.size()));
    for (const auto& property : properties) {
        protocol::writeString(output, property.name);
        protocol::writeString(output, property.value);
        protocol::writeBoolean(output, property.signature.has_value());
        if (property.signature)
            protocol::writeString(output, *property.signature);
    }
    const auto tag = hmac(secret, output.mutableSpan(offset).subspan(kSignatureLength));
    if (!tag) {
        output.trimTail(output.writerIndex() - offset);
        return std::unexpected(EncodeError::kCryptoFailed);
    }
    std::copy(tag->begin(), tag->end(), output.mutableSpan(offset).begin());
    return {};
}

} // namespace basalt::wire::auth::forwarding::velocity
