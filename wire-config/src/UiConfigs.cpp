#include <basalt/wire/common/placeholder/Placeholders.hpp>
#include <basalt/wire/config/UiConfigs.hpp>

#include <algorithm>
#include <array>
#include <cstdint>
#include <utility>

namespace basalt::wire::config {
namespace {

using wire::common::Logger;
using wire::common::placeholder::lowercaseAscii;
using wire::common::toml::TomlTable;
using wire::protocol::play::BossBarColor;
using wire::protocol::play::BossBarOverlay;

constexpr std::array<BossBarColor, 7> kColors{BossBarColor::Pink,  BossBarColor::Blue,   BossBarColor::Red,
                                              BossBarColor::Green, BossBarColor::Yellow, BossBarColor::Purple,
                                              BossBarColor::White};
constexpr std::array<BossBarOverlay, 5> kOverlays{BossBarOverlay::Progress, BossBarOverlay::Notched6,
                                                  BossBarOverlay::Notched10, BossBarOverlay::Notched12,
                                                  BossBarOverlay::Notched20};

[[nodiscard]] ProfileHeader emptyDefault() {
    return ProfileHeader{.name = "default", .priority = 0, .isDefault = true, .isActive = false, .conditions = {}};
}

[[nodiscard]] std::int32_t intervalOf(const TomlTable& root) {
    return static_cast<std::int32_t>(std::max<std::int64_t>(root.integer("update-interval-ticks").value_or(20), 0));
}

[[nodiscard]] UiPriority priorityOf(const TomlTable& root, const std::string& path, UiPriority fallback,
                                    const Logger& logger) {
    const TomlTable* ui = root.table("ui");
    const std::optional<std::string_view> written = ui == nullptr ? std::nullopt : ui->string("priority");
    if (!written.has_value()) {
        return fallback;
    }
    const std::string lowercase = lowercaseAscii(*written);
    if (lowercase == "proxy") {
        return UiPriority::Proxy;
    }
    if (lowercase == "backend") {
        return UiPriority::Backend;
    }
    logger.warn([&path, lowercase] {
        return path + " has [ui] priority = '" + lowercase + "'; it must be proxy or backend, using the default.";
    });
    return fallback;
}

} // namespace

std::string_view uiPriorityName(UiPriority priority) noexcept {
    return priority == UiPriority::Proxy ? "proxy" : "backend";
}

std::string_view bossBarColorName(BossBarColor color) noexcept {
    switch (color) {
    case BossBarColor::Pink:
        return "pink";
    case BossBarColor::Blue:
        return "blue";
    case BossBarColor::Red:
        return "red";
    case BossBarColor::Green:
        return "green";
    case BossBarColor::Yellow:
        return "yellow";
    case BossBarColor::Purple:
        return "purple";
    case BossBarColor::White:
    default:
        return "white";
    }
}

TabListProfile TabListProfile::inheriting(const TabListProfile& fallback) const {
    if (&fallback == this) {
        return *this;
    }
    TabListProfile filled = *this;
    if (!filled.headerLines.has_value()) {
        filled.headerLines = fallback.headerLines;
    }
    if (!filled.footerLines.has_value()) {
        filled.footerLines = fallback.footerLines;
    }
    return filled;
}

TabListConfig TabListConfig::disabled() {
    TabListConfig config;
    config.profiles.push_back(TabListProfile{.header = emptyDefault(), .headerLines = {}, .footerLines = {}});
    return config;
}

std::expected<TabListConfig, std::string> TabListConfig::load(const ConfigDirectory& directory,
                                                              std::string_view defaultContents,
                                                              UiPriority fallbackPriority,
                                                              const wire::common::Logger& logger) {
    const std::string path = directory.pathOf(kFileName);
    const std::expected<TomlTable, std::string> root = directory.load(kFileName, defaultContents);
    if (!root.has_value()) {
        return std::unexpected(root.error());
    }
    auto read = parseProfiles(*root, path, [](ProfileHeader header, const TomlTable& table) {
        return TabListProfile{.header = std::move(header),
                              .headerLines = linesOf(table, "header"),
                              .footerLines = linesOf(table, "footer")};
    });
    if (!read.has_value()) {
        return std::unexpected(read.error());
    }
    const bool enabled = root->boolean("enabled").value_or(true);
    if (!enabled) {
        logger.debug([&path] { return path + " is switched off."; });
    }
    return TabListConfig{.enabled = enabled,
                         .priority = priorityOf(*root, path, fallbackPriority, logger),
                         .updateIntervalTicks = intervalOf(*root),
                         .profiles = std::move(read->first),
                         .defaultIndex = read->second};
}

ScoreboardProfile ScoreboardProfile::inheriting(const ScoreboardProfile& fallback) const {
    if (&fallback == this) {
        return *this;
    }
    ScoreboardProfile filled = *this;
    if (!filled.title.has_value()) {
        filled.title = fallback.title;
    }
    if (!filled.lines.has_value()) {
        filled.lines = fallback.lines;
    }
    return filled;
}

ScoreboardConfig ScoreboardConfig::disabled() {
    ScoreboardConfig config;
    config.profiles.push_back(ScoreboardProfile{.header = emptyDefault(), .title = {}, .lines = {}});
    return config;
}

std::expected<ScoreboardConfig, std::string> ScoreboardConfig::load(const ConfigDirectory& directory,
                                                                    std::string_view defaultContents,
                                                                    UiPriority fallbackPriority,
                                                                    const wire::common::Logger& logger) {
    const std::string path = directory.pathOf(kFileName);
    const std::expected<TomlTable, std::string> root = directory.load(kFileName, defaultContents);
    if (!root.has_value()) {
        return std::unexpected(root.error());
    }
    auto read = parseProfiles(*root, path, [](ProfileHeader header, const TomlTable& table) {
        std::optional<std::string> title;
        if (const std::optional<std::string_view> written = table.string("title"); written.has_value()) {
            title = std::string{*written};
        }
        return ScoreboardProfile{
            .header = std::move(header), .title = std::move(title), .lines = linesOf(table, "lines")};
    });
    if (!read.has_value()) {
        return std::unexpected(read.error());
    }
    for (const ScoreboardProfile& profile : read->first) {
        const std::size_t count = profile.lines.has_value() ? profile.lines->size() : 0;
        if (count > static_cast<std::size_t>(kMaxLines)) {
            logger.warn([&profile, &path, count] {
                return "Scoreboard profile '" + profile.header.name + "' in " + path + " has " + std::to_string(count) +
                       " lines; the client draws " + std::to_string(kMaxLines) + ", so the rest are dropped.";
            });
        }
    }
    return ScoreboardConfig{.enabled = root->boolean("enabled").value_or(false),
                            .priority = priorityOf(*root, path, fallbackPriority, logger),
                            .updateIntervalTicks = intervalOf(*root),
                            .profiles = std::move(read->first),
                            .defaultIndex = read->second};
}

BossBarProfile BossBarProfile::inheriting(const BossBarProfile& fallback) const {
    if (&fallback == this) {
        return *this;
    }
    BossBarProfile filled = *this;
    if (!filled.text.has_value()) {
        filled.text = fallback.text;
    }
    if (!filled.color.has_value()) {
        filled.color = fallback.color;
    }
    if (!filled.overlay.has_value()) {
        filled.overlay = fallback.overlay;
    }
    if (!filled.progress.has_value()) {
        filled.progress = fallback.progress;
    }
    return filled;
}

BossBarConfig BossBarConfig::disabled() {
    BossBarConfig config;
    config.profiles.push_back(
        BossBarProfile{.header = emptyDefault(), .text = {}, .color = {}, .overlay = {}, .progress = {}});
    return config;
}

std::expected<BossBarConfig, std::string> BossBarConfig::load(const ConfigDirectory& directory,
                                                              std::string_view defaultContents,
                                                              UiPriority fallbackPriority,
                                                              const wire::common::Logger& logger) {
    const std::string path = directory.pathOf(kFileName);
    const std::expected<TomlTable, std::string> root = directory.load(kFileName, defaultContents);
    if (!root.has_value()) {
        return std::unexpected(root.error());
    }
    auto read = parseProfiles(*root, path, [&path, &logger](ProfileHeader header, const TomlTable& table) {
        std::optional<std::string> text;
        if (const std::optional<std::string_view> written = table.string("text"); written.has_value()) {
            text = std::string{*written};
        }
        std::optional<BossBarColor> color;
        if (const std::optional<std::string_view> written = table.string("color"); written.has_value()) {
            const std::string lowercase = lowercaseAscii(*written);
            const auto found = std::ranges::find_if(
                kColors, [&lowercase](BossBarColor candidate) { return bossBarColorName(candidate) == lowercase; });
            // An unknown colour is reported and falls back: one wrong decoration word is not an unreadable file.
            if (found == kColors.end()) {
                const std::string name = header.name;
                const std::string value{*written};
                logger.warn([&path, name, value] {
                    return "Boss bar profile '" + name + "' in " + path + " has an unknown colour '" + value +
                           "'; using white.";
                });
                color = BossBarColor::White;
            } else {
                color = *found;
            }
        }
        std::optional<BossBarOverlay> overlay;
        if (const std::optional<std::string_view> written = table.string("overlay"); written.has_value()) {
            const std::string lowercase = lowercaseAscii(*written);
            const auto found = std::ranges::find_if(kOverlays, [&lowercase](BossBarOverlay candidate) {
                return wire::protocol::play::bossBarOverlayName(candidate) == lowercase;
            });
            if (found == kOverlays.end()) {
                const std::string name = header.name;
                const std::string value{*written};
                logger.warn([&path, name, value] {
                    return "Boss bar profile '" + name + "' in " + path + " has an unknown overlay '" + value +
                           "'; using progress.";
                });
                overlay = BossBarOverlay::Progress;
            } else {
                overlay = *found;
            }
        }
        std::optional<std::string> progress;
        if (const std::optional<std::string_view> written = table.string("progress"); written.has_value()) {
            progress = std::string{*written};
        } else if (const std::optional<std::int64_t> number = table.integer("progress"); number.has_value()) {
            progress = std::to_string(*number);
        }
        return BossBarProfile{.header = std::move(header),
                              .text = std::move(text),
                              .color = color,
                              .overlay = overlay,
                              .progress = std::move(progress)};
    });
    if (!read.has_value()) {
        return std::unexpected(read.error());
    }
    return BossBarConfig{.enabled = root->boolean("enabled").value_or(false),
                         .priority = priorityOf(*root, path, fallbackPriority, logger),
                         .updateIntervalTicks = intervalOf(*root),
                         .profiles = std::move(read->first),
                         .defaultIndex = read->second};
}

} // namespace basalt::wire::config
