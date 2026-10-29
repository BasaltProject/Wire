#pragma once

#include <basalt/wire/common/logging/Logger.hpp>
#include <basalt/wire/config/ConfigDirectory.hpp>
#include <basalt/wire/config/ProfileSelection.hpp>
#include <basalt/wire/protocol/play/UiPackets.hpp>

#include <cstdint>
#include <expected>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace basalt::wire::config {

/// Who owns the element when a backend sends its own: the proxy, or the backend that is in charge of the player.
enum class UiPriority : std::uint8_t { kProxy, kBackend };

[[nodiscard]] std::string_view uiPriorityName(UiPriority priority) noexcept;

struct TabListProfile {
    ProfileHeader header;
    std::optional<std::vector<std::string>> headerLines;
    std::optional<std::vector<std::string>> footerLines;

    [[nodiscard]] TabListProfile inheriting(const TabListProfile& fallback) const;
};

struct TabListConfig {
    static constexpr std::string_view kFileName = "tab_list.toml";

    bool enabled = false;
    UiPriority priority = UiPriority::kProxy;
    std::int32_t updateIntervalTicks = 0;
    std::vector<TabListProfile> profiles;
    std::size_t defaultIndex = 0;

    [[nodiscard]] static TabListConfig disabled();

    [[nodiscard]] static std::expected<TabListConfig, std::string>
    load(const ConfigDirectory& directory, std::string_view defaultContents,
         UiPriority fallbackPriority = UiPriority::kProxy,
         const wire::common::Logger& logger = wire::common::Logger{"Config"});

    [[nodiscard]] const TabListProfile& defaultProfile() const { return profiles[defaultIndex]; }
};

struct ScoreboardProfile {
    ProfileHeader header;
    std::optional<std::string> title;
    std::optional<std::vector<std::string>> lines;

    [[nodiscard]] ScoreboardProfile inheriting(const ScoreboardProfile& fallback) const;
};

struct ScoreboardConfig {
    static constexpr std::string_view kFileName = "scoreboard.toml";

    /// The client only draws this many rows; more would be written and never seen.
    static constexpr std::int32_t kMaxLines = 15;

    bool enabled = false;
    UiPriority priority = UiPriority::kProxy;
    std::int32_t updateIntervalTicks = 0;
    std::vector<ScoreboardProfile> profiles;
    std::size_t defaultIndex = 0;

    [[nodiscard]] static ScoreboardConfig disabled();

    [[nodiscard]] static std::expected<ScoreboardConfig, std::string>
    load(const ConfigDirectory& directory, std::string_view defaultContents,
         UiPriority fallbackPriority = UiPriority::kProxy,
         const wire::common::Logger& logger = wire::common::Logger{"Config"});

    [[nodiscard]] const ScoreboardProfile& defaultProfile() const { return profiles[defaultIndex]; }
};

struct BossBarProfile {
    ProfileHeader header;
    std::optional<std::string> text;
    std::optional<wire::protocol::play::BossBarColor> color;
    std::optional<wire::protocol::play::BossBarOverlay> overlay;
    std::optional<std::string> progress;

    [[nodiscard]] BossBarProfile inheriting(const BossBarProfile& fallback) const;
};

struct BossBarConfig {
    static constexpr std::string_view kFileName = "boss_bar.toml";

    bool enabled = false;
    UiPriority priority = UiPriority::kProxy;
    std::int32_t updateIntervalTicks = 0;
    std::vector<BossBarProfile> profiles;
    std::size_t defaultIndex = 0;

    [[nodiscard]] static BossBarConfig disabled();

    [[nodiscard]] static std::expected<BossBarConfig, std::string>
    load(const ConfigDirectory& directory, std::string_view defaultContents,
         UiPriority fallbackPriority = UiPriority::kProxy,
         const wire::common::Logger& logger = wire::common::Logger{"Config"});

    [[nodiscard]] const BossBarProfile& defaultProfile() const { return profiles[defaultIndex]; }
};

[[nodiscard]] std::string_view bossBarColorName(wire::protocol::play::BossBarColor color) noexcept;

struct UiConfigs {
    TabListConfig tabList = TabListConfig::disabled();
    ScoreboardConfig scoreboard = ScoreboardConfig::disabled();
    BossBarConfig bossBar = BossBarConfig::disabled();

    [[nodiscard]] bool anyEnabled() const noexcept { return tabList.enabled || scoreboard.enabled || bossBar.enabled; }
};

} // namespace basalt::wire::config
