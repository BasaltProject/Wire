#include <basalt/wire/common/Base64.hpp>

#include <array>
#include <cstdint>

namespace basalt::wire::common::base64 {
namespace {

constexpr std::string_view kAlphabet = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

constexpr std::array<std::int8_t, 256> buildReverse() {
    std::array<std::int8_t, 256> table{};
    for (std::size_t i = 0; i < table.size(); ++i) {
        table[i] = -1;
    }
    for (std::size_t i = 0; i < kAlphabet.size(); ++i) {
        table[static_cast<std::size_t>(kAlphabet[i])] = static_cast<std::int8_t>(i);
    }
    return table;
}

constexpr std::array<std::int8_t, 256> kReverse = buildReverse();

} // namespace

std::string encode(std::span<const std::uint8_t> bytes) {
    std::string out;
    out.reserve(((bytes.size() + 2) / 3) * 4);

    std::size_t i = 0;
    for (; i + 3 <= bytes.size(); i += 3) {
        const std::uint32_t block = (static_cast<std::uint32_t>(bytes[i]) << 16) |
                                    (static_cast<std::uint32_t>(bytes[i + 1]) << 8) | bytes[i + 2];
        out += kAlphabet[(block >> 18) & 0x3F];
        out += kAlphabet[(block >> 12) & 0x3F];
        out += kAlphabet[(block >> 6) & 0x3F];
        out += kAlphabet[block & 0x3F];
    }

    const std::size_t remaining = bytes.size() - i;
    if (remaining == 1) {
        const std::uint32_t block = static_cast<std::uint32_t>(bytes[i]) << 16;
        out += kAlphabet[(block >> 18) & 0x3F];
        out += kAlphabet[(block >> 12) & 0x3F];
        out += "==";
    } else if (remaining == 2) {
        const std::uint32_t block =
            (static_cast<std::uint32_t>(bytes[i]) << 16) | (static_cast<std::uint32_t>(bytes[i + 1]) << 8);
        out += kAlphabet[(block >> 18) & 0x3F];
        out += kAlphabet[(block >> 12) & 0x3F];
        out += kAlphabet[(block >> 6) & 0x3F];
        out += '=';
    }
    return out;
}

std::optional<std::vector<std::uint8_t>> decode(std::string_view text) {
    std::vector<std::uint8_t> out;
    out.reserve((text.size() / 4) * 3);

    std::uint32_t block = 0;
    int digits = 0;
    for (const char c : text) {
        if (c == '=' || c == '\n' || c == '\r') {
            continue;
        }
        const std::int8_t value = kReverse[static_cast<std::uint8_t>(c)];
        if (value < 0) {
            return std::nullopt;
        }
        block = (block << 6) | static_cast<std::uint32_t>(value);
        if (++digits == 4) {
            out.push_back(static_cast<std::uint8_t>(block >> 16));
            out.push_back(static_cast<std::uint8_t>(block >> 8));
            out.push_back(static_cast<std::uint8_t>(block));
            block = 0;
            digits = 0;
        }
    }

    if (digits == 2) {
        out.push_back(static_cast<std::uint8_t>(block >> 4));
    } else if (digits == 3) {
        out.push_back(static_cast<std::uint8_t>(block >> 10));
        out.push_back(static_cast<std::uint8_t>(block >> 2));
    } else if (digits != 0) {
        return std::nullopt;
    }
    return out;
}

} // namespace basalt::wire::common::base64

namespace basalt::wire::common::base64url {

std::string encode(std::span<const std::uint8_t> bytes) {
    std::string text = base64::encode(bytes);
    while (!text.empty() && text.back() == '=') {
        text.pop_back();
    }
    for (char& character : text) {
        if (character == '+') {
            character = '-';
        } else if (character == '/') {
            character = '_';
        }
    }
    return text;
}

std::optional<std::vector<std::uint8_t>> decode(std::string_view text) {
    std::string standard{text};
    for (char& character : standard) {
        if (character == '-') {
            character = '+';
        } else if (character == '_') {
            character = '/';
        }
    }
    return base64::decode(standard);
}

} // namespace basalt::wire::common::base64url
