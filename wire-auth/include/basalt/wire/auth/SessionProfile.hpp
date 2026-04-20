#pragma once

#include <basalt/wire/common/Uuid.hpp>

#include <optional>
#include <string>
#include <vector>

namespace basalt::wire::auth {

struct SessionProfileProperty {
    std::string name;
    std::string value;
    std::optional<std::string> signature;

    friend bool operator==(const SessionProfileProperty&, const SessionProfileProperty&) = default;
};

struct SessionProfile {
    common::Uuid id;
    std::string name;
    std::vector<SessionProfileProperty> properties;

    friend bool operator==(const SessionProfile&, const SessionProfile&) = default;
};

/// parity: MinecraftSessionService.hasJoinedServer.
class SessionService {
public:
    SessionService() = default;
    SessionService(const SessionService&) = default;
    SessionService(SessionService&&) = default;
    SessionService& operator=(const SessionService&) = default;
    SessionService& operator=(SessionService&&) = default;
    virtual ~SessionService() = default;

    [[nodiscard]] virtual std::optional<SessionProfile>
    hasJoined(std::string_view username, std::string_view serverHash, std::string_view clientAddress) = 0;
};

} // namespace basalt::wire::auth
