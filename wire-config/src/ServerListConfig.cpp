#include <basalt/wire/config/ServerListConfig.hpp>

#include <algorithm>
#include <cstdint>
#include <utility>

namespace basalt::wire::config {

using wire::common::toml::TomlTable;

const wire::common::placeholder::PlaceholderSlot<PingRequest>& pingSlot() {
    static const wire::common::placeholder::PlaceholderSlot<PingRequest> slot{"ping"};
    return slot;
}

const std::optional<std::string>& PingRequest::knownPlayer() const {
    if (!asked_) {
        asked_ = true;
        if (lookup_) {
            knownPlayer_ = lookup_();
        }
    }
    return knownPlayer_;
}

ServerListProfile ServerListProfile::from(ProfileHeader header, const TomlTable& table) {
    const auto text = [&table](std::string_view key) -> std::optional<std::string> {
        const std::optional<std::string_view> written = table.string(key);
        return written.has_value() ? std::optional<std::string>{std::string{*written}} : std::nullopt;
    };
    const auto number = [&table](std::string_view key) -> std::optional<std::int32_t> {
        const std::optional<std::int64_t> written = table.integer(key);
        return written.has_value() ? std::optional<std::int32_t>{static_cast<std::int32_t>(*written)} : std::nullopt;
    };
    std::optional<std::vector<std::string>> hover;
    if (std::vector<std::string> written = table.stringList("hover"); !written.empty()) {
        hover = std::move(written);
    }
    return ServerListProfile{.header = std::move(header),
                             .motd = linesOf(table, "motd"),
                             .versionName = text("version-name"),
                             .versionProtocol = number("version-protocol"),
                             .hidePlayerCount = table.boolean("hide-player-count"),
                             .hideOnlinePlayers = table.boolean("hide-online-players"),
                             .sampleSize = number("sample-size"),
                             .maxPlayers = number("max-players"),
                             .onlinePlayers = number("online-players"),
                             .hover = std::move(hover),
                             .icon = text("icon")};
}

ServerListProfile ServerListProfile::inheriting(const ServerListProfile& fallback) const {
    if (&fallback == this) {
        return *this;
    }
    ServerListProfile filled = *this;
    if (!filled.motd.has_value()) {
        filled.motd = fallback.motd;
    }
    if (!filled.versionName.has_value()) {
        filled.versionName = fallback.versionName;
    }
    if (!filled.versionProtocol.has_value()) {
        filled.versionProtocol = fallback.versionProtocol;
    }
    if (!filled.hidePlayerCount.has_value()) {
        filled.hidePlayerCount = fallback.hidePlayerCount;
    }
    if (!filled.hideOnlinePlayers.has_value()) {
        filled.hideOnlinePlayers = fallback.hideOnlinePlayers;
    }
    if (!filled.sampleSize.has_value()) {
        filled.sampleSize = fallback.sampleSize;
    }
    if (!filled.maxPlayers.has_value()) {
        filled.maxPlayers = fallback.maxPlayers;
    }
    if (!filled.onlinePlayers.has_value()) {
        filled.onlinePlayers = fallback.onlinePlayers;
    }
    if (!filled.hover.has_value()) {
        filled.hover = fallback.hover;
    }
    if (!filled.icon.has_value()) {
        filled.icon = fallback.icon;
    }
    return filled;
}

const ServerListProfile* ServerListConfig::named(std::string_view name) const {
    const auto found = std::ranges::find(
        profiles_, name, [](const ServerListProfile& profile) -> const std::string& { return profile.header.name; });
    return found == profiles_.end() ? nullptr : &*found;
}

ServerListProfile ServerListConfig::select(const wire::common::placeholder::PlaceholderRegistry& registry,
                                           const wire::common::placeholder::PlaceholderContext& context,
                                           const wire::common::Logger& logger) const {
    const ServerListProfile& chosen =
        selectProfile(profiles_, defaultProfile(), registry, context, logger, *reporter_, "server list");
    return chosen.inheriting(defaultProfile());
}

std::expected<ServerListConfig, std::string> ServerListConfig::load(const ConfigDirectory& directory,
                                                                    std::string_view defaultContents,
                                                                    const wire::common::Logger& logger) {
    const std::string path = directory.pathOf(kFileName);
    const std::expected<TomlTable, std::string> root = directory.load(kFileName, defaultContents);
    if (!root.has_value()) {
        return std::unexpected(root.error());
    }
    auto read = parseProfiles(*root, path, [](ProfileHeader header, const TomlTable& table) {
        return ServerListProfile::from(std::move(header), table);
    });
    if (!read.has_value()) {
        return std::unexpected(read.error());
    }

    // Named at startup: a server showing its maintenance message because a flag was left on is hard to debug.
    const ServerListProfile* active = nullptr;
    for (const ServerListProfile& profile : read->first) {
        if (profile.header.isActive && (active == nullptr || profile.header.priority > active->header.priority)) {
            active = &profile;
        }
    }
    if (active != nullptr) {
        const std::string name = active->header.name;
        logger.info([&path, name] { return "Server list profile '" + name + "' is switched on in " + path + "."; });
    }

    const TomlTable* greeting = root->table("greeting");
    const bool rememberIp = greeting == nullptr ? false : greeting->boolean("remember-ip").value_or(false);
    return ServerListConfig{std::move(read->first), read->second, rememberIp};
}

} // namespace basalt::wire::config
