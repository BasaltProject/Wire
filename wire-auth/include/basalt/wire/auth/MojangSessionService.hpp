#pragma once

#include <basalt/wire/auth/HttpsClient.hpp>
#include <basalt/wire/auth/SessionProfile.hpp>

#include <functional>
#include <string>

namespace basalt::wire::auth {

inline constexpr std::size_t kMaxSessionProfileBytes = 64 * 1024;

/// parity: MinecraftSessionService.hasJoinedServer: a profile means authenticated, a 204 or anything else means not.
class MojangSessionService final : public SessionService {
public:
    using Fetch = std::function<std::expected<HttpsResponse, HttpsError>(std::string_view url)>;

    explicit MojangSessionService(Fetch fetch = {});

    [[nodiscard]] std::optional<SessionProfile> hasJoined(std::string_view username, std::string_view serverHash,
                                                          std::string_view clientAddress) override;

private:
    Fetch fetch_;
};

[[nodiscard]] std::optional<std::string> mojangUuidOf(const std::string& name,
                                                      const MojangSessionService::Fetch& fetch = {});

} // namespace basalt::wire::auth
