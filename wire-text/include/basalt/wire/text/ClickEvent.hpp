#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

namespace basalt::wire::text {

/// What happens when a player clicks a piece of text. Mirrors Adventure's ClickEvent model.
struct ClickEvent {
    enum class Action : std::uint8_t {
        OpenUrl,
        RunCommand,
        SuggestCommand,
        ChangePage,
        CopyToClipboard,
    };

    Action action = Action::RunCommand;
    std::string value;

    [[nodiscard]] static ClickEvent openUrl(std::string url) { return {Action::OpenUrl, std::move(url)}; }

    [[nodiscard]] static ClickEvent runCommand(std::string command) { return {Action::RunCommand, std::move(command)}; }

    [[nodiscard]] static ClickEvent suggestCommand(std::string command) {
        return {Action::SuggestCommand, std::move(command)};
    }

    [[nodiscard]] static ClickEvent changePage(std::int32_t page);

    [[nodiscard]] static ClickEvent copyToClipboard(std::string text) {
        return {Action::CopyToClipboard, std::move(text)};
    }

    friend bool operator==(const ClickEvent&, const ClickEvent&) = default;
};

[[nodiscard]] std::string_view clickActionId(ClickEvent::Action action) noexcept;

[[nodiscard]] std::optional<ClickEvent::Action> clickActionFromId(std::string_view id) noexcept;

} // namespace basalt::wire::text
