#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

namespace basalt::wire::text {

class TextColor {
public:
    constexpr explicit TextColor(std::int32_t rgb) noexcept : rgb_(rgb) {}

    [[nodiscard]] constexpr std::int32_t rgb() const noexcept { return rgb_; }

    [[nodiscard]] constexpr std::int32_t red() const noexcept { return (rgb_ >> 16) & 0xFF; }

    [[nodiscard]] constexpr std::int32_t green() const noexcept { return (rgb_ >> 8) & 0xFF; }

    [[nodiscard]] constexpr std::int32_t blue() const noexcept { return rgb_ & 0xFF; }

    [[nodiscard]] std::string toHexString() const;

    [[nodiscard]] static constexpr TextColor of(std::int32_t red, std::int32_t green, std::int32_t blue) noexcept {
        return TextColor{((red & 0xFF) << 16) | ((green & 0xFF) << 8) | (blue & 0xFF)};
    }

    [[nodiscard]] static std::optional<TextColor> fromHex(std::string_view hex);

    [[nodiscard]] static std::optional<TextColor> named(std::string_view name);

    [[nodiscard]] static std::optional<std::string_view> nameOf(TextColor color);

    [[nodiscard]] static std::optional<TextColor> parse(std::string_view text);

    [[nodiscard]] static TextColor lerp(TextColor from, TextColor to, float fraction) noexcept;

    friend constexpr bool operator==(TextColor, TextColor) = default;

private:
    std::int32_t rgb_;
};

inline constexpr TextColor kBlack{0x000000};
inline constexpr TextColor kDarkBlue{0x0000AA};
inline constexpr TextColor kDarkGreen{0x00AA00};
inline constexpr TextColor kDarkAqua{0x00AAAA};
inline constexpr TextColor kDarkRed{0xAA0000};
inline constexpr TextColor kDarkPurple{0xAA00AA};
inline constexpr TextColor kGold{0xFFAA00};
inline constexpr TextColor kGray{0xAAAAAA};
inline constexpr TextColor kDarkGray{0x555555};
inline constexpr TextColor kBlue{0x5555FF};
inline constexpr TextColor kGreen{0x55FF55};
inline constexpr TextColor kAqua{0x55FFFF};
inline constexpr TextColor kRed{0xFF5555};
inline constexpr TextColor kLightPurple{0xFF55FF};
inline constexpr TextColor kYellow{0xFFFF55};
inline constexpr TextColor kWhite{0xFFFFFF};

} // namespace basalt::wire::text
