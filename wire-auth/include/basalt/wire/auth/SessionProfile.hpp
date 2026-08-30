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

/// Verifies a client's claimed username against Mojang's session server after the key exchange.
///
/// parity: MinecraftSessionService.hasJoinedServer. An implementation backed by the real session
/// server needs an HTTPS client, which lands with libcurl; until then the runtime supplies its own,
/// and a test supplies a fake.
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
