#include <basalt/wire/common/placeholder/PlaceholderText.hpp>
#include <basalt/wire/config/DefaultIcon.hpp>
#include <basalt/wire/config/ServerListStatus.hpp>
#include <basalt/wire/io/FileIO.hpp>
#include <basalt/wire/text/MiniMessage.hpp>
#include <basalt/wire/text/Serializers.hpp>

#include <algorithm>
#include <array>
#include <cstdint>
#include <utility>

namespace basalt::wire::config {
namespace {

namespace ph = wire::common::placeholder;

using wire::protocol::status::ServerStatusPayload;
using wire::text::Component;

constexpr std::int32_t kIconSize = 64;
// parity: `MinecraftServer.buildPlayerStatus`, twelve names.
constexpr std::int32_t kSampleLimit = 12;

// Hover lines use a nil UUID: the field is not optional and a made-up one would be a real player's id.
constexpr std::string_view kNilUuid = "00000000-0000-0000-0000-000000000000";

[[nodiscard]] std::int32_t readInt(const std::vector<std::uint8_t>& bytes, std::size_t offset) {
    return static_cast<std::int32_t>(
        (static_cast<std::uint32_t>(bytes[offset]) << 24) | (static_cast<std::uint32_t>(bytes[offset + 1]) << 16) |
        (static_cast<std::uint32_t>(bytes[offset + 2]) << 8) | static_cast<std::uint32_t>(bytes[offset + 3]));
}

[[nodiscard]] bool isPng64(const std::vector<std::uint8_t>& bytes) {
    static constexpr std::array<std::uint8_t, 8> kSignature{0x89, 'P', 'N', 'G', 0x0D, 0x0A, 0x1A, 0x0A};
    if (bytes.size() < 24) {
        return false;
    }
    for (std::size_t index = 0; index < kSignature.size(); index++) {
        if (bytes[index] != kSignature[index]) {
            return false;
        }
    }
    return readInt(bytes, 16) == kIconSize && readInt(bytes, 20) == kIconSize;
}

} // namespace

ServerListStatus::ServerListStatus(std::function<std::shared_ptr<const ServerListConfig>()> config,
                                   const ph::PlaceholderRegistry& placeholders,
                                   std::function<std::vector<ListedPlayer>()> onlinePlayers, Options options,
                                   Hooks hooks, wire::common::Logger logger)
    : config_(std::move(config)), placeholders_(&placeholders), onlinePlayers_(std::move(onlinePlayers)),
      options_(std::move(options)), hooks_(std::move(hooks)), logger_(std::move(logger)) {
    if (options_.builtInIcon.empty()) {
        const std::span<const std::uint8_t> icon = defaultIcon::bytes();
        options_.builtInIcon.assign(icon.begin(), icon.end());
    }
}

ServerStatusPayload ServerListStatus::statusFor(std::int32_t protocolVersion, const std::string& host,
                                                const std::optional<std::string>& address) {
    const std::shared_ptr<const ServerListConfig> held = config_();
    const ServerListConfig& current = *held;
    const PingRequest request{protocolVersion, host, address, PingRequest::Lookup{[this, &current, &address] {
                                  return current.rememberIp() && address.has_value() && hooks_.playerByAddress
                                             ? hooks_.playerByAddress(*address)
                                             : std::nullopt;
                              }}};
    const ph::PlaceholderContext context = ph::PlaceholderContext{}.with(pingSlot(), &request);
    const ServerListProfile* loading = isLoading() ? current.named("loading") : nullptr;
    const ServerListProfile profile = loading != nullptr ? loading->inheriting(current.defaultProfile())
                                                         : current.select(*placeholders_, context, logger_);
    const std::vector<ListedPlayer> players = onlinePlayers_ ? onlinePlayers_() : std::vector<ListedPlayer>{};

    std::optional<ServerStatusPayload> relayed;
    std::vector<std::string> relaying;
    if (profile.source.has_value() && *profile.source != "profile" && hooks_.relay) {
        relaying = profile.relay.value_or(std::vector<std::string>{"online", "max", "sample"});
        relayed = hooks_.relay(*profile.source, host, relaying);
    }
    const auto relays = [&relayed, &relaying](std::string_view field) {
        return relayed.has_value() && std::ranges::find(relaying, field) != relaying.end();
    };

    std::int32_t online = profile.onlinePlayers.value_or(static_cast<std::int32_t>(players.size()));
    std::int32_t max = profile.maxPlayers.value_or(options_.slots);
    if (relays("online") && relayed->players.has_value()) {
        online = relayed->players->online;
    }
    if (relays("max") && relayed->players.has_value()) {
        max = relayed->players->max;
    }
    Component motd =
        relays("motd") ? relayed->description : render(profile.motd.value_or(std::vector<std::string>{}), context);

    if (hooks_.ping) {
        const Hooks::Answer answer = hooks_.ping(address, wire::text::toPlainText(motd), online, max);
        if (answer.motd.has_value()) {
            motd = wire::text::miniMessage::parse(*answer.motd);
        }
        if (answer.maxPlayers.has_value()) {
            max = *answer.maxPlayers;
        }
    }

    ServerStatusPayload payload;
    payload.description = std::move(motd);
    if (!profile.hidePlayerCount.value_or(false)) {
        payload.players =
            ServerStatusPayload::Players{.max = max, .online = online, .sample = sampleFor(profile, players, context)};
        if (relays("sample") && relayed->players.has_value()) {
            payload.players->sample = relayed->players->sample;
        }
    }
    payload.version =
        ServerStatusPayload::Version{.name = profile.versionName.value_or(options_.versionName),
                                     .protocol = profile.versionProtocol.value_or(options_.protocolVersion)};
    if (relays("version") && relayed->version.has_value()) {
        payload.version = *relayed->version;
    }
    if (relays("icon") && relayed->favicon.has_value()) {
        payload.favicon = relayed->favicon;
    } else if (std::vector<std::uint8_t> icon = iconFor(profile); !icon.empty()) {
        payload.favicon = std::move(icon);
    }
    return payload;
}

std::vector<ServerStatusPayload::SamplePlayer>
ServerListStatus::sampleFor(const ServerListProfile& profile, const std::vector<ListedPlayer>& players,
                            const ph::PlaceholderContext& context) const {
    std::vector<ServerStatusPayload::SamplePlayer> sample;
    if (profile.hover.has_value()) {
        for (const std::string& line : *profile.hover) {
            sample.push_back({.name = substitute(line, context), .id = std::string{kNilUuid}});
        }
        return sample;
    }
    // parity: `MinecraftServer.buildPlayerStatus` with `hide-online-players`, the count stays and the names go.
    if (profile.hideOnlinePlayers.value_or(false)) {
        return sample;
    }
    for (const ListedPlayer& player : players) {
        if (sample.size() >= static_cast<std::size_t>(std::max(profile.sampleSize.value_or(kSampleLimit), 0))) {
            break;
        }
        sample.push_back({.name = player.name, .id = player.uuid});
    }
    return sample;
}

std::vector<std::uint8_t> ServerListStatus::iconFor(const ServerListProfile& profile) const {
    if (!profile.icon.has_value()) {
        return options_.builtInIcon;
    }
    const std::string& name = *profile.icon;
    {
        const std::lock_guard<std::mutex> guard{mutex_};
        if (const auto found = icons_.find(name); found != icons_.end()) {
            return found->second.empty() ? options_.builtInIcon : found->second;
        }
    }

    const std::string path = options_.configRoot + "/" + name;
    const std::optional<std::vector<std::uint8_t>> bytes = wire::io::fileIO::readBytes(path);
    std::vector<std::uint8_t> usable;
    if (!bytes.has_value()) {
        logger_.warn([&path, &name] {
            return "server_list.toml icon \"" + name + "\": " + path + " not found; using the built-in one.";
        });
    } else if (!isPng64(*bytes)) {
        logger_.warn([&path, &name] {
            return "server_list.toml icon \"" + name + "\": " + path + " is not a 64x64 PNG; using the built-in one.";
        });
    } else {
        usable = *bytes;
    }
    {
        const std::lock_guard<std::mutex> guard{mutex_};
        icons_.insert_or_assign(name, usable);
    }
    return usable.empty() ? options_.builtInIcon : usable;
}

Component ServerListStatus::render(const std::vector<std::string>& lines, const ph::PlaceholderContext& context) const {
    if (lines.empty()) {
        return Component::empty();
    }
    Component result = wire::text::miniMessage::parse(substitute(lines.front(), context));
    for (std::size_t index = 1; index < lines.size(); index++) {
        result = result.append(Component::text("\n"))
                     .append(wire::text::miniMessage::parse(substitute(lines[index], context)));
    }
    return result;
}

std::string ServerListStatus::substitute(const std::string& text, const ph::PlaceholderContext& context) const {
    // Filled in before MiniMessage parses; unknown names are warned about, unanswerable ones resolve to nothing.
    return ph::substitute(
        text, *placeholders_, context,
        [this](std::string_view name, const ph::PlaceholderValue& value) -> std::optional<std::string> {
            if (value.kind == ph::PlaceholderKind::Unknown) {
                reportOnce("The server list uses %" + std::string{name} + "%, which is not a placeholder known here.");
                return std::nullopt;
            }
            logger_.debug([&name, &value] {
                return "Server list placeholder %" + std::string{name} +
                       "% is empty here: " + value.reason.value_or("no reason given") + ".";
            });
            return std::string{};
        });
}

void ServerListStatus::reportOnce(const std::string& message) const {
    {
        const std::lock_guard<std::mutex> guard{mutex_};
        if (!reported_.insert(message).second) {
            return;
        }
    }
    logger_.warn([&message] { return message; });
}

} // namespace basalt::wire::config
