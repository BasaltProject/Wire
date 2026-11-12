#include <basalt/wire/bedrock/NetherNetStun.hpp>

#include <algorithm>
#include <basalt_mbedtls_threads.h>
#include <cstring>
#include <mbedtls/md.h>
#include <zlib.h>

namespace basalt::wire::bedrock {
namespace {

constexpr std::uint32_t kMagicCookie = 0x2112a442U;
constexpr std::uint32_t kFingerprintXor = 0x5354554eU;
constexpr std::uint16_t kBindingRequest = 0x0001;
constexpr std::uint16_t kBindingSuccess = 0x0101;
constexpr std::uint16_t kBindingError = 0x0111;

constexpr std::uint16_t kAttrUsername = 0x0006;
constexpr std::uint16_t kAttrMessageIntegrity = 0x0008;
constexpr std::uint16_t kAttrErrorCode = 0x0009;
constexpr std::uint16_t kAttrXorMappedAddress = 0x0020;
constexpr std::uint16_t kAttrUseCandidate = 0x0025;
constexpr std::uint16_t kAttrFingerprint = 0x8028;

constexpr std::size_t kIntegritySize = 20;

std::uint16_t readU16(const std::uint8_t* at) { return static_cast<std::uint16_t>((at[0] << 8U) | at[1]); }

std::uint32_t readU32(const std::uint8_t* at) {
    return (static_cast<std::uint32_t>(at[0]) << 24U) | (static_cast<std::uint32_t>(at[1]) << 16U) |
           (static_cast<std::uint32_t>(at[2]) << 8U) | at[3];
}

void writeU16(std::vector<std::uint8_t>& out, std::uint16_t value) {
    out.push_back(static_cast<std::uint8_t>(value >> 8U));
    out.push_back(static_cast<std::uint8_t>(value));
}

void writeU32(std::vector<std::uint8_t>& out, std::uint32_t value) {
    writeU16(out, static_cast<std::uint16_t>(value >> 16U));
    writeU16(out, static_cast<std::uint16_t>(value));
}

void setLength(std::vector<std::uint8_t>& message, std::size_t length) {
    message[2] = static_cast<std::uint8_t>(length >> 8U);
    message[3] = static_cast<std::uint8_t>(length);
}

bool hmacSha1(std::string_view key, std::span<const std::uint8_t> data, std::uint8_t* out) {
    basalt_mbedtls_threading_init();
    const mbedtls_md_info_t* info = mbedtls_md_info_from_type(MBEDTLS_MD_SHA1);
    return info != nullptr && mbedtls_md_hmac(info, reinterpret_cast<const unsigned char*>(key.data()), key.size(),
                                              data.data(), data.size(), out) == 0;
}

std::uint32_t fingerprintOf(std::span<const std::uint8_t> data) {
    return static_cast<std::uint32_t>(::crc32(0, data.data(), static_cast<uInt>(data.size()))) ^ kFingerprintXor;
}

void startMessage(std::vector<std::uint8_t>& out, std::uint16_t type, const StunTransactionId& transaction) {
    out.clear();
    writeU16(out, type);
    writeU16(out, 0);
    writeU32(out, kMagicCookie);
    out.insert(out.end(), transaction.begin(), transaction.end());
}

void padTo4(std::vector<std::uint8_t>& out) {
    while (out.size() % 4 != 0) {
        out.push_back(0);
    }
}

/// Appends integrity then fingerprint, the way every message this server sends ends.
void sign(std::vector<std::uint8_t>& message, std::string_view password) {
    setLength(message, message.size() - kStunHeaderSize + 4 + kIntegritySize);
    std::array<std::uint8_t, kIntegritySize> mac{};
    hmacSha1(password, message, mac.data());
    writeU16(message, kAttrMessageIntegrity);
    writeU16(message, kIntegritySize);
    message.insert(message.end(), mac.begin(), mac.end());

    setLength(message, message.size() - kStunHeaderSize + 8);
    const std::uint32_t fingerprint = fingerprintOf(message);
    writeU16(message, kAttrFingerprint);
    writeU16(message, 4);
    writeU32(message, fingerprint);
}

} // namespace

bool looksLikeStun(std::span<const std::uint8_t> datagram) noexcept {
    return datagram.size() >= kStunHeaderSize && (datagram[0] & 0xc0U) == 0 &&
           readU32(datagram.data() + 4) == kMagicCookie;
}

std::optional<std::string> stunUsername(std::span<const std::uint8_t> datagram) {
    if (!looksLikeStun(datagram)) {
        return std::nullopt;
    }
    std::size_t offset = kStunHeaderSize;
    while (offset + 4 <= datagram.size()) {
        const std::uint16_t type = readU16(datagram.data() + offset);
        const std::size_t length = readU16(datagram.data() + offset + 2);
        if (offset + 4 + length > datagram.size()) {
            return std::nullopt;
        }
        if (type == kAttrUsername) {
            return std::string{reinterpret_cast<const char*>(datagram.data() + offset + 4), length};
        }
        offset += 4 + ((length + 3U) & ~3U);
    }
    return std::nullopt;
}

std::optional<StunBindingRequest> parseStunBindingRequest(std::span<const std::uint8_t> datagram,
                                                          std::string_view localPassword) {
    if (!looksLikeStun(datagram) || readU16(datagram.data()) != kBindingRequest) {
        return std::nullopt;
    }
    const std::size_t bodyLength = readU16(datagram.data() + 2);
    if (bodyLength % 4 != 0 || datagram.size() != kStunHeaderSize + bodyLength) {
        return std::nullopt;
    }

    StunBindingRequest request;
    std::copy_n(datagram.begin() + 8, request.transaction.size(), request.transaction.begin());

    std::optional<std::size_t> integrityAt;
    std::optional<std::size_t> fingerprintAt;
    std::size_t offset = kStunHeaderSize;
    while (offset + 4 <= datagram.size()) {
        const std::uint16_t type = readU16(datagram.data() + offset);
        const std::size_t length = readU16(datagram.data() + offset + 2);
        const std::size_t valueAt = offset + 4;
        if (valueAt + length > datagram.size()) {
            return std::nullopt;
        }
        switch (type) {
        case kAttrUsername:
            request.username.assign(reinterpret_cast<const char*>(datagram.data() + valueAt), length);
            break;
        case kAttrUseCandidate:
            request.useCandidate = true;
            break;
        case kAttrMessageIntegrity:
            if (length != kIntegritySize) {
                return std::nullopt;
            }
            integrityAt = offset;
            break;
        case kAttrFingerprint:
            if (length != 4) {
                return std::nullopt;
            }
            fingerprintAt = offset;
            break;
        default:
            break;
        }
        offset = valueAt + ((length + 3U) & ~3U);
    }

    if (!integrityAt) {
        return std::nullopt;
    }

    // The integrity covers the message as if it ended at its own attribute, so the length field is rewritten.
    std::vector<std::uint8_t> covered(datagram.begin(), datagram.begin() + static_cast<std::ptrdiff_t>(*integrityAt));
    setLength(covered, *integrityAt - kStunHeaderSize + 4 + kIntegritySize);
    std::array<std::uint8_t, kIntegritySize> mac{};
    if (!hmacSha1(localPassword, covered, mac.data()) ||
        !std::equal(mac.begin(), mac.end(), datagram.begin() + static_cast<std::ptrdiff_t>(*integrityAt + 4))) {
        return std::nullopt;
    }

    if (fingerprintAt) {
        std::vector<std::uint8_t> head(datagram.begin(),
                                       datagram.begin() + static_cast<std::ptrdiff_t>(*fingerprintAt));
        setLength(head, *fingerprintAt - kStunHeaderSize + 8);
        if (readU32(datagram.data() + *fingerprintAt + 4) != fingerprintOf(head)) {
            return std::nullopt;
        }
    }
    return request;
}

std::vector<std::uint8_t> buildStunBindingSuccess(const StunTransactionId& transaction, const io::UdpEndpoint& mapped,
                                                  std::string_view localPassword) {
    std::vector<std::uint8_t> message;
    startMessage(message, kBindingSuccess, transaction);

    // The endpoint address is kept in network byte order, the port in host order.
    const std::uint32_t address = mapped.address;
    writeU16(message, kAttrXorMappedAddress);
    writeU16(message, 8);
    message.push_back(0);
    message.push_back(0x01);
    writeU16(message, static_cast<std::uint16_t>(mapped.port ^ (kMagicCookie >> 16U)));
    std::uint8_t bytes[4];
    std::memcpy(bytes, &address, sizeof(bytes));
    for (std::size_t i = 0; i < 4; ++i) {
        message.push_back(
            static_cast<std::uint8_t>(bytes[i] ^ static_cast<std::uint8_t>(kMagicCookie >> (24U - 8U * i))));
    }
    sign(message, localPassword);
    return message;
}

std::vector<std::uint8_t> buildStunBindingError(const StunTransactionId& transaction, std::uint16_t code,
                                                std::string_view reason) {
    std::vector<std::uint8_t> message;
    startMessage(message, kBindingError, transaction);
    writeU16(message, kAttrErrorCode);
    writeU16(message, static_cast<std::uint16_t>(4 + reason.size()));
    writeU16(message, 0);
    message.push_back(static_cast<std::uint8_t>(code / 100));
    message.push_back(static_cast<std::uint8_t>(code % 100));
    message.insert(message.end(), reason.begin(), reason.end());
    padTo4(message);
    setLength(message, message.size() - kStunHeaderSize);
    return message;
}

} // namespace basalt::wire::bedrock
