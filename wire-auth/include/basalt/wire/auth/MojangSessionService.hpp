#pragma once

#include <basalt/wire/auth/HttpsClient.hpp>
#include <basalt/wire/auth/SessionProfile.hpp>

#include <functional>
#include <string>

namespace basalt::wire::auth {

/// Profiles are small JSON documents, not the jars fetched by other HTTPS callers.
inline constexpr std::size_t kMaxSessionProfileBytes = 64 * 1024;

/// The real session service, against Mojang's own endpoint.
///
/// parity: `MinecraftSessionService.hasJoinedServer`, which is one `GET` carrying the username and
/// the hash the key exchange produced. A profile back means the client really is who it says; a 204
/// with no body means it is not, and so does anything else.
class MojangSessionService final : public SessionService {
public:
    /// The fetch is a callback so a test can answer without a network, and so this file stays the
    /// only one that knows the endpoint's shape.
    using Fetch = std::function<std::expected<HttpsResponse, HttpsError>(std::string_view url)>;

    explicit MojangSessionService(Fetch fetch = {});

    [[nodiscard]] std::optional<SessionProfile> hasJoined(std::string_view username, std::string_view serverHash,
                                                          std::string_view clientAddress) override;

private:
    Fetch fetch_;
};

/// Name to uuid against Mojang's public profile API, which is what `/fetchprofile` answers with.
///
/// The dashless form the API returns is expanded, because every other uuid this server prints is
/// dashed and two spellings of the same id in the same console is a needless puzzle.
[[nodiscard]] std::optional<std::string> mojangUuidOf(const std::string& name,
                                                      const MojangSessionService::Fetch& fetch = {});

} // namespace basalt::wire::auth
