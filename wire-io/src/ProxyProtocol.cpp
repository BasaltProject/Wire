#include <basalt/wire/io/ProxyProtocol.hpp>

#include <algorithm>
#include <array>
#include <charconv>
#include <string_view>

namespace basalt::wire::io::proxyProtocol {
namespace {
constexpr std::array<std::uint8_t, 12> kV2Signature{0x0D, 0x0A, 0x0D, 0x0A, 0x00, 0x0D,
                                                    0x0A, 0x51, 0x55, 0x49, 0x54, 0x0A};
constexpr std::string_view kV1Prefix = "PROXY ";
constexpr std::size_t kV1MaxLength = 107;
constexpr std::size_t kV2MaxPayload = 536;

Result invalid() { return Result{Status::kInvalid, 0, {}, 0, 0}; }

Result more(std::size_t needed) { return Result{Status::kNeedMore, 0, {}, 0, needed}; }

std::string formatIpv6(std::span<const std::uint8_t> bytes) {
    constexpr std::string_view kHex = "0123456789abcdef";
    std::string text;
    for (std::size_t group = 0; group < 8; ++group) {
        if (group != 0)
            text += ':';
        const unsigned value = (static_cast<unsigned>(bytes[group * 2]) << 8) | bytes[group * 2 + 1];
        for (int shift = 12; shift >= 0; shift -= 4)
            text += kHex[(value >> shift) & 0xF];
    }
    return text;
}

template <typename Expected>
bool matchesPrefix(std::span<const std::uint8_t> bytes, const Expected& expected) {
    const auto count = std::min(bytes.size(), std::size(expected));
    return std::equal(bytes.begin(), bytes.begin() + static_cast<std::ptrdiff_t>(count), std::begin(expected),
                      [](std::uint8_t a, auto b) { return a == static_cast<std::uint8_t>(b); });
}

Result parseV1(std::span<const std::uint8_t> bytes) {
    const auto limit = std::min(bytes.size(), kV1MaxLength);
    std::size_t end = 0;
    while (end + 1 < limit && !(bytes[end] == '\r' && bytes[end + 1] == '\n'))
        ++end;
    if (end + 1 >= limit)
        return bytes.size() >= kV1MaxLength ? invalid() : more(bytes.size() + 1);
    const std::string text{bytes.begin(), bytes.begin() + static_cast<std::ptrdiff_t>(end)};
    const std::string_view line{text};
    std::array<std::string_view, 6> fields;
    std::size_t count = 0;
    std::string_view rest = line;
    while (true) {
        const auto space = rest.find(' ');
        if (count == fields.size())
            return invalid();
        fields[count++] = rest.substr(0, space);
        if (space == std::string_view::npos)
            break;
        rest.remove_prefix(space + 1);
    }
    Result done{Status::kDone, end + 2, {}, 0, 0};
    if (count >= 2 && fields[1] == "UNKNOWN")
        return done;
    if (count != 6 || (fields[1] != "TCP4" && fields[1] != "TCP6") || fields[2].empty())
        return invalid();
    unsigned port = 0;
    const auto digits = fields[4];
    const auto [stop, error] = std::from_chars(digits.data(), digits.data() + digits.size(), port);
    if (error != std::errc{} || stop != digits.data() + digits.size() || port > 65535)
        return invalid();
    done.address = std::string{fields[2]};
    done.port = static_cast<std::uint16_t>(port);
    return done;
}

Result parseV2(std::span<const std::uint8_t> bytes) {
    if (bytes.size() < 16)
        return more(16);
    const std::uint8_t versionCommand = bytes[12];
    const std::uint8_t family = bytes[13];
    const std::size_t length = (static_cast<std::size_t>(bytes[14]) << 8) | bytes[15];
    if ((versionCommand >> 4) != 2 || length > kV2MaxPayload || (versionCommand & 0x0F) > 1)
        return invalid();
    if (bytes.size() < 16 + length)
        return more(16 + length);
    const auto payload = bytes.subspan(16, length);
    Result done{Status::kDone, 16 + length, {}, 0, 0};
    if ((versionCommand & 0x0F) == 0)
        return done;
    const std::uint8_t addressFamily = family >> 4;
    if (addressFamily == 1 && length >= 12) {
        done.address = std::to_string(payload[0]) + "." + std::to_string(payload[1]) + "." +
                       std::to_string(payload[2]) + "." + std::to_string(payload[3]);
        done.port = static_cast<std::uint16_t>((payload[8] << 8) | payload[9]);
    } else if (addressFamily == 2 && length >= 36) {
        done.address = formatIpv6(payload.first(16));
        done.port = static_cast<std::uint16_t>((payload[32] << 8) | payload[33]);
    }
    return done;
}
} // namespace

Result parse(std::span<const std::uint8_t> bytes) {
    if (bytes.empty())
        return more(kV1Prefix.size());
    if (bytes[0] == kV2Signature[0]) {
        if (!matchesPrefix(bytes, kV2Signature))
            return invalid();
        return bytes.size() < kV2Signature.size() ? more(kV2Signature.size()) : parseV2(bytes);
    }
    if (!matchesPrefix(bytes, kV1Prefix))
        return invalid();
    return bytes.size() < kV1Prefix.size() ? more(kV1Prefix.size()) : parseV1(bytes);
}

} // namespace basalt::wire::io::proxyProtocol
