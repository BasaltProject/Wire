#include <basalt/wire/auth/LoginAuthentication.hpp>
#include <basalt/wire/auth/MojangSessionService.hpp>
#include <basalt/wire/auth/SessionKeyPair.hpp>

#include <algorithm>
#include <utility>

namespace basalt::wire::auth {
namespace {
using Error = LoginAuthenticationError;
using Failure = LoginAuthenticationFailure;

std::optional<Failure> interrupted(const OnlineLoginRequest& request, std::stop_token stop,
                                   const std::optional<LoginSecret>& secret = {}) {
    if (stop.stop_requested())
        return Failure{Error::Canceled, secret};
    if (std::chrono::steady_clock::now() >= request.deadline)
        return Failure{Error::TimedOut, secret};
    return std::nullopt;
}

bool sameName(std::string_view a, std::string_view b) {
    if (a.size() != b.size())
        return false;
    const auto lower = [](char c) { return c >= 'A' && c <= 'Z' ? static_cast<char>(c + ('a' - 'A')) : c; };
    for (std::size_t i = 0; i < a.size(); ++i) {
        if (lower(a[i]) != lower(b[i]))
            return false;
    }
    return true;
}
} // namespace

bool validLoginName(std::string_view name) noexcept {
    return !name.empty() && name.size() <= 16 && std::ranges::all_of(name, [](char c) {
        const auto byte = static_cast<unsigned char>(c);
        return byte > 32 && byte < 127;
    });
}

std::expected<AuthenticatedLogin, Failure> authenticateLogin(const SessionKeyPair& key,
                                                             const OnlineLoginRequest& request, std::stop_token stop,
                                                             const AuthenticationFetch& fetch) {
    if (auto failed = interrupted(request, stop))
        return std::unexpected(*failed);
    if (!validLoginName(request.name) || request.address.size() > 64 ||
        std::ranges::any_of(request.address,
                            [](char c) {
                                const auto byte = static_cast<unsigned char>(c);
                                return byte <= 32 || byte == 127;
                            }) ||
        request.encryptedSecret.size() != 128 || request.encryptedChallenge.size() != 128)
        return std::unexpected(Failure{Error::InvalidRequest, {}});
    const auto challenge = key.decrypt(request.encryptedChallenge);
    if (!challenge || challenge->size() != request.challenge.size())
        return std::unexpected(Failure{Error::InvalidChallenge, {}});
    std::uint8_t mismatch = 0;
    for (std::size_t i = 0; i < request.challenge.size(); ++i)
        mismatch |= static_cast<std::uint8_t>((*challenge)[i] ^ request.challenge[i]);
    if (mismatch != 0)
        return std::unexpected(Failure{Error::InvalidChallenge, {}});
    if (auto failed = interrupted(request, stop))
        return std::unexpected(*failed);
    const auto decrypted = key.decrypt(request.encryptedSecret);
    if (!decrypted || decrypted->size() != 16)
        return std::unexpected(Failure{Error::InvalidKey, {}});
    LoginSecret secret{};
    std::ranges::copy(*decrypted, secret.begin());
    if (auto failed = interrupted(request, stop, secret))
        return std::unexpected(*failed);
    const auto hash = sessionHash("", secret, key.publicKeyDer());
    std::optional<Error> transportError;
    MojangSessionService service{[&](std::string_view url) {
        auto response =
            fetch ? fetch(url, request.deadline, stop)
                  : httpsGetBounded(url, kMaxSessionProfileBytes, {.deadline = request.deadline, .stop = stop});
        if (!response) {
            switch (response.error().kind) {
            case HttpsError::Kind::Canceled:
                transportError = Error::Canceled;
                break;
            case HttpsError::Kind::TimedOut:
                transportError = Error::TimedOut;
                break;
            default:
                transportError = Error::Unavailable;
                break;
            }
        } else if (response->status == 429 || response->status >= 500) {
            transportError = Error::Unavailable;
        }
        return response;
    }};
    std::optional<SessionProfile> profile;
    try {
        profile = service.hasJoined(request.name, hash, request.address);
    } catch (...) {
        return std::unexpected(Failure{Error::Unavailable, secret});
    }
    if (auto failed = interrupted(request, stop, secret))
        return std::unexpected(*failed);
    if (!profile)
        return std::unexpected(Failure{transportError.value_or(Error::Rejected), secret});
    if (profile->id.isNil() || !validLoginName(profile->name) || !sameName(request.name, profile->name))
        return std::unexpected(Failure{Error::Rejected, secret});
    if (profile->properties.size() > 64)
        return std::unexpected(Failure{Error::Rejected, secret});
    std::size_t bytes = profile->name.size();
    for (const auto& property : profile->properties) {
        const auto signature = property.signature ? property.signature->size() : 0;
        if (property.name.size() > 256 || property.value.size() > 32768 || signature > 8192)
            return std::unexpected(Failure{Error::Rejected, secret});
        bytes += property.name.size() + property.value.size() + signature;
        if (bytes > kMaxSessionProfileBytes)
            return std::unexpected(Failure{Error::Rejected, secret});
    }
    return AuthenticatedLogin{secret, std::move(*profile)};
}

} // namespace basalt::wire::auth
