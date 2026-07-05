#pragma once

#include <basalt/wire/common/logging/Logger.hpp>
#include <basalt/wire/config/ConfigDirectory.hpp>
#include <basalt/wire/config/ProfileSelection.hpp>

#include <cstdint>
#include <expected>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace basalt::wire::config {

class PingRequest {
public:
    using Lookup = std::function<std::optional<std::string>()>;

    PingRequest(std::int32_t protocolVersion, std::string host, std::optional<std::string> address,
                Lookup lookupKnownPlayer = {})
        : protocolVersion_(protocolVersion), host_(std::move(host)), address_(std::move(address)),
          lookup_(std::move(lookupKnownPlayer)) {}

    PingRequest(std::int32_t protocolVersion, std::string host, std::optional<std::string> address,
                std::optional<std::string> knownPlayer)
        : protocolVersion_(protocolVersion), host_(std::move(host)), address_(std::move(address)),
          knownPlayer_(std::move(knownPlayer)), asked_(true) {}

    [[nodiscard]] std::int32_t protocolVersion() const noexcept { return protocolVersion_; }

    [[nodiscard]] const std::string& host() const noexcept { return host_; }

    [[nodiscard]] const std::optional<std::string>& address() const noexcept { return address_; }

    [[nodiscard]] const std::optional<std::string>& knownPlayer() const;

private:
    std::int32_t protocolVersion_ = 0;
    std::string host_;
    std::optional<std::string> address_;
    Lookup lookup_;
    mutable std::optional<std::string> knownPlayer_;
    mutable bool asked_ = false;
};

[[nodiscard]] const wire::common::placeholder::PlaceholderSlot<PingRequest>& pingSlot();

struct ServerListProfile {
    ProfileHeader header;
    std::optional<std::vector<std::string>> motd;
    std::optional<std::string> versionName;
    std::optional<std::int32_t> versionProtocol;
    std::optional<bool> hidePlayerCount;
    std::optional<bool> hideOnlinePlayers;
    std::optional<std::int32_t> sampleSize;
    std::optional<std::int32_t> maxPlayers;
    std::optional<std::int32_t> onlinePlayers;
    std::optional<std::vector<std::string>> hover;
    std::optional<std::string> icon;
    std::optional<std::string> source;
    std::optional<std::vector<std::string>> relay;

    [[nodiscard]] static ServerListProfile from(ProfileHeader header, const wire::common::toml::TomlTable& table);

    [[nodiscard]] ServerListProfile inheriting(const ServerListProfile& fallback) const;
};

class ServerListConfig {
public:
    static constexpr std::string_view kFileName = "server_list.toml";

    ServerListConfig(std::vector<ServerListProfile> profiles, std::size_t defaultIndex, bool rememberIp)
        : profiles_(std::move(profiles)), defaultIndex_(defaultIndex), rememberIp_(rememberIp),
          reporter_(std::make_unique<OnceReporter>()) {}

    [[nodiscard]] static std::expected<ServerListConfig, std::string>
    load(const ConfigDirectory& directory, std::string_view defaultContents,
         const wire::common::Logger& logger = wire::common::Logger{"Config"});

    [[nodiscard]] const std::vector<ServerListProfile>& profiles() const noexcept { return profiles_; }

    [[nodiscard]] const ServerListProfile& defaultProfile() const { return profiles_[defaultIndex_]; }

    [[nodiscard]] const ServerListProfile* named(std::string_view name) const;

    [[nodiscard]] bool rememberIp() const noexcept { return rememberIp_; }

    [[nodiscard]] ServerListProfile select(
        const wire::common::placeholder::PlaceholderRegistry& registry,
        const wire::common::placeholder::PlaceholderContext& context = wire::common::placeholder::PlaceholderContext{},
        const wire::common::Logger& logger = wire::common::Logger{"ServerList"}) const;

private:
    std::vector<ServerListProfile> profiles_;
    std::size_t defaultIndex_ = 0;
    bool rememberIp_ = false;
    /// Held indirectly so the configuration stays movable: the reporter owns a mutex.
    std::unique_ptr<OnceReporter> reporter_;
};

} // namespace basalt::wire::config
