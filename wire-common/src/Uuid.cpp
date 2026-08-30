#include <basalt/wire/common/Uuid.hpp>
#include <basalt/wire/common/hash/Md5.hpp>

#include <string>

namespace basalt::wire::common {
namespace {

constexpr char kDigits[] = "0123456789abcdef";

void appendHex(std::string& out, std::uint64_t value, int digits) {
    for (int i = digits - 1; i >= 0; --i) {
        out += kDigits[(value >> (i * 4)) & 0xF];
    }
}

std::optional<std::uint8_t> hexDigit(char c) noexcept {
    if (c >= '0' && c <= '9') {
        return static_cast<std::uint8_t>(c - '0');
    }
    if (c >= 'a' && c <= 'f') {
        return static_cast<std::uint8_t>(c - 'a' + 10);
    }
    if (c >= 'A' && c <= 'F') {
        return static_cast<std::uint8_t>(c - 'A' + 10);
    }
    return std::nullopt;
}

} // namespace

std::string Uuid::toString() const {
    std::string out;
    out.reserve(36);
    appendHex(out, high_ >> 32, 8);
    out += '-';
    appendHex(out, (high_ >> 16) & 0xFFFF, 4);
    out += '-';
    appendHex(out, high_ & 0xFFFF, 4);
    out += '-';
    appendHex(out, low_ >> 48, 4);
    out += '-';
    appendHex(out, low_ & 0xFFFFFFFFFFFFULL, 12);
    return out;
}

std::string Uuid::toUndashedString() const {
    std::string out;
    out.reserve(32);
    appendHex(out, high_, 16);
    appendHex(out, low_, 16);
    return out;
}

std::optional<Uuid> Uuid::parse(std::string_view text) {
    std::uint64_t high = 0;
    std::uint64_t low = 0;
    int digits = 0;

    for (const char c : text) {
        if (c == '-') {
            continue;
        }
        const std::optional<std::uint8_t> digit = hexDigit(c);
        if (!digit || digits >= 32) {
            return std::nullopt;
        }
        if (digits < 16) {
            high = (high << 4) | *digit;
        } else {
            low = (low << 4) | *digit;
        }
        ++digits;
    }
    if (digits != 32) {
        return std::nullopt;
    }
    return Uuid{high, low};
}

Uuid Uuid::nameBased(std::string_view input) {
    std::array<std::uint8_t, 16> digest = md5(input);
    // Version 3 in the high nibble of byte 6, variant 2 in the top bits of byte 8.
    digest[6] = static_cast<std::uint8_t>((digest[6] & 0x0F) | 0x30);
    digest[8] = static_cast<std::uint8_t>((digest[8] & 0x3F) | 0x80);

    std::uint64_t high = 0;
    std::uint64_t low = 0;
    for (std::size_t i = 0; i < 8; ++i) {
        high = (high << 8) | digest[i];
    }
    for (std::size_t i = 8; i < 16; ++i) {
        low = (low << 8) | digest[i];
    }
    return Uuid{high, low};
}

Uuid Uuid::offlinePlayer(std::string_view name) { return nameBased("OfflinePlayer:" + std::string{name}); }

} // namespace basalt::wire::common
