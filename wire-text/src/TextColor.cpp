#include <basalt/wire/text/TextColor.hpp>

#include <algorithm>
#include <array>
#include <charconv>
#include <cstdint>
#include <utility>

namespace basalt::wire::text {
namespace {

// Order is vanilla's colour order, which is also the legacy code order.
constexpr std::array<std::pair<std::string_view, TextColor>, 16> kNamed = {{
    {"black", kBlack},
    {"dark_blue", kDarkBlue},
    {"dark_green", kDarkGreen},
    {"dark_aqua", kDarkAqua},
    {"dark_red", kDarkRed},
    {"dark_purple", kDarkPurple},
    {"gold", kGold},
    {"gray", kGray},
    {"dark_gray", kDarkGray},
    {"blue", kBlue},
    {"green", kGreen},
    {"aqua", kAqua},
    {"red", kRed},
    {"light_purple", kLightPurple},
    {"yellow", kYellow},
    {"white", kWhite},
}};

} // namespace

std::string TextColor::toHexString() const {
    static constexpr char kDigits[] = "0123456789abcdef";
    std::string out = "#000000";
    for (int i = 0; i < 6; ++i) {
        out[static_cast<std::size_t>(6 - i)] = kDigits[(rgb_ >> (i * 4)) & 0xF];
    }
    return out;
}

std::optional<TextColor> TextColor::fromHex(std::string_view hex) {
    if (hex.starts_with('#')) {
        hex.remove_prefix(1);
    }
    if (hex.size() != 6) {
        return std::nullopt;
    }
    std::uint32_t value = 0;
    const auto result = std::from_chars(hex.data(), hex.data() + hex.size(), value, 16);
    if (result.ec != std::errc{} || result.ptr != hex.data() + hex.size()) {
        return std::nullopt;
    }
    return TextColor{static_cast<std::int32_t>(value)};
}

std::optional<TextColor> TextColor::named(std::string_view name) {
    for (const auto& [key, color] : kNamed) {
        if (key == name) {
            return color;
        }
    }
    return std::nullopt;
}

std::optional<std::string_view> TextColor::nameOf(TextColor color) {
    for (const auto& [key, value] : kNamed) {
        if (value == color) {
            return key;
        }
    }
    return std::nullopt;
}

std::optional<TextColor> TextColor::parse(std::string_view text) {
    if (const std::optional<TextColor> byName = named(text)) {
        return byName;
    }
    return fromHex(text);
}

TextColor TextColor::lerp(TextColor from, TextColor to, float fraction) noexcept {
    const float t = std::clamp(fraction, 0.0F, 1.0F);
    const auto mix = [t](std::int32_t a, std::int32_t b) {
        return std::clamp(static_cast<std::int32_t>(static_cast<float>(a) + static_cast<float>(b - a) * t), 0, 255);
    };
    return of(mix(from.red(), to.red()), mix(from.green(), to.green()), mix(from.blue(), to.blue()));
}

} // namespace basalt::wire::text
