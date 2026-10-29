#pragma once

#include <basalt/wire/common/Uuid.hpp>
#include <basalt/wire/common/logging/Logger.hpp>
#include <basalt/wire/common/placeholder/Placeholders.hpp>
#include <basalt/wire/config/ProfileSelection.hpp>
#include <basalt/wire/config/UiConfigs.hpp>
#include <basalt/wire/io/ByteBuffer.hpp>
#include <basalt/wire/protocol/play/UiPackets.hpp>

#include <array>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace basalt::wire::config {

enum class UiElement : std::uint8_t { kTabList, kScoreboard, kBossBar };

struct SurfaceContent {
    std::string title;
    std::vector<std::string> lines;
};

class PlayerUi {
public:
    using Send = std::function<void(std::int32_t packetId, const std::function<void(wire::io::ByteBuffer&)>& write)>;

    struct Overrides {
        std::function<bool(const wire::common::placeholder::PlaceholderContext&)> holdsTabList;
        std::function<bool()> holdsSidebar;
        std::function<std::optional<SurfaceContent>()> sidebar;
        std::function<bool()> holdsBossBar;
        std::function<std::optional<SurfaceContent>()> bossBar;
    };

    struct Surfaces {
        std::function<void(const std::string& title, const std::vector<std::string>& lines)> scoreboard;
        std::function<void()> clearScoreboard;
        std::function<void(const std::string& title, float progress)> bossBar;
        std::function<void()> clearBossBar;
    };

    PlayerUi(std::function<std::shared_ptr<const UiConfigs>()> configs,
             const wire::common::placeholder::PlaceholderRegistry& placeholders, Send send, Overrides overrides = {},
             wire::common::Logger logger = wire::common::Logger{"Ui"});

    void tick(std::int64_t tick, const wire::common::placeholder::PlaceholderContext& context);

    void clear();

    void worldChanged();

    /// Drops what is remembered as shown without sending anything: the client has lost it already.
    void forget();

    /// Takes the element off the client and stops drawing it until forget(), because the backend now owns it.
    void yield(UiElement element);

    [[nodiscard]] bool yielded(UiElement element) const noexcept { return yielded_[static_cast<std::size_t>(element)]; }

    void drawInto(Surfaces surfaces) { surfaces_ = std::move(surfaces); }

private:
    void updateTabList(const wire::common::placeholder::PlaceholderContext& context);

    void updateScoreboard(const wire::common::placeholder::PlaceholderContext& context);

    void drawScoreboard(const std::string& title, const std::vector<std::string>& lines);

    void clearScoreboard();

    void updateBossBar(const wire::common::placeholder::PlaceholderContext& context);

    void drawBossBar(const std::string& text, float progress, wire::protocol::play::BossBarColor color,
                     wire::protocol::play::BossBarOverlay overlay);

    void clearBossBar();

    [[nodiscard]] float progressOf(const std::optional<std::string>& raw,
                                   const wire::common::placeholder::PlaceholderContext& context);

    [[nodiscard]] std::pair<std::string, wire::text::Component>
    render(const std::vector<std::string>& lines, const wire::common::placeholder::PlaceholderContext& context);

    [[nodiscard]] std::string substitute(std::string_view text,
                                         const wire::common::placeholder::PlaceholderContext& context);

    std::function<std::shared_ptr<const UiConfigs>()> configs_;
    const wire::common::placeholder::PlaceholderRegistry* placeholders_;
    Send send_;
    Surfaces surfaces_;
    Overrides overrides_;
    wire::common::Logger logger_;
    OnceReporter reporter_;

    std::optional<std::pair<std::string, std::string>> lastTabList_;
    bool redrawPending_ = false;
    std::array<bool, 3> yielded_{};
    std::optional<std::string> lastScoreboardTitle_;
    std::vector<std::string> lastScoreboardLines_;
    bool scoreboardShown_ = false;
    std::optional<wire::common::Uuid> bossBarId_;
    std::optional<std::string> lastBossText_;
    std::optional<float> lastBossProgress_;
    std::optional<std::pair<wire::protocol::play::BossBarColor, wire::protocol::play::BossBarOverlay>> lastBossStyle_;
};

} // namespace basalt::wire::config
