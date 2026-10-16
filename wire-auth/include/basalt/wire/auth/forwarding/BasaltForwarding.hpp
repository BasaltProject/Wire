#pragma once

#include <basalt/wire/auth/SessionProfile.hpp>
#include <basalt/wire/common/Uuid.hpp>

#include <array>
#include <cstdint>
#include <expected>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

namespace basalt::wire::auth::forwarding::native {

inline constexpr std::string_view kChannel = "basalt:forwarding";
inline constexpr std::string_view kSigningDomain = "basalt:forwarding/1";

inline constexpr std::uint8_t kVersion1 = 1;
inline constexpr std::size_t kChallengeSize = 32;
inline constexpr std::size_t kPublicKeySize = 32;
inline constexpr std::size_t kSignatureSize = 64;
inline constexpr std::size_t kMaxResponseSize = 128 * 1024;

using PublicKey = std::array<std::uint8_t, kPublicKeySize>;
using Challenge = std::array<std::uint8_t, kChallengeSize>;

/// Sixteen lowercase hex characters: the first eight bytes of SHA-256 over the raw public key.
[[nodiscard]] std::string keyId(const PublicKey& key);

class SigningKey {
public:
    SigningKey(const SigningKey&) = default;
    SigningKey& operator=(const SigningKey&) = default;
    ~SigningKey();

    [[nodiscard]] static std::optional<SigningKey> generate();
    [[nodiscard]] static SigningKey fromSeed(std::span<const std::uint8_t, 32> seed);

    [[nodiscard]] std::span<const std::uint8_t, 32> seed() const noexcept;
    [[nodiscard]] const PublicKey& publicKey() const noexcept { return public_; }

    [[nodiscard]] std::array<std::uint8_t, kSignatureSize> signature(std::span<const std::uint8_t> message) const;

private:
    SigningKey() = default;

    std::array<std::uint8_t, 64> secret_{};
    PublicKey public_{};
};

/// Backend to proxy: the backend names itself and fixes the challenge, so a signature fits one connection.
struct Request {
    std::uint8_t versionMin = kVersion1;
    std::uint8_t versionMax = kVersion1;
    Challenge challenge{};
    std::string backendId;

    friend bool operator==(const Request&, const Request&) = default;
};

enum class Origin : std::uint8_t { Java = 0, Bedrock = 1, Other = 2 };

struct Identity {
    common::Uuid profileId;
    std::string name;
    std::vector<SessionProfileProperty> properties;
    std::string address;
    std::uint16_t port = 0;
    std::string virtualHost;
    std::uint16_t virtualPort = 0;
    bool onlineMode = false;
    std::string proxyId;
    Origin origin = Origin::Java;
    std::string originId;

    friend bool operator==(const Identity&, const Identity&) = default;
};

struct Verified {
    std::uint8_t version = 0;
    std::string keyId;
    Identity identity;
};

/// A rejection's reason is for the log only, never for the player.
using VerifyResult = std::variant<Verified, std::string>;

[[nodiscard]] std::optional<Request> newRequest(std::string_view backendId);

[[nodiscard]] std::vector<std::uint8_t> encodeRequest(const Request& request);

[[nodiscard]] std::optional<Request> decodeRequest(std::span<const std::uint8_t> data);

/// Empty when the identity exceeds a limit or the version is outside the request's range.
[[nodiscard]] std::vector<std::uint8_t> sign(const SigningKey& key, const Request& request, std::uint8_t version,
                                             const Identity& identity);

[[nodiscard]] VerifyResult verify(std::span<const std::uint8_t> response, const Request& issued,
                                  std::span<const PublicKey> trusted);

} // namespace basalt::wire::auth::forwarding::native
