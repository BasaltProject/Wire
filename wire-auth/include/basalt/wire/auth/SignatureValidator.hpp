#pragma once

#include <basalt/wire/common/Uuid.hpp>

#include <cstdint>
#include <memory>
#include <optional>
#include <span>
#include <string_view>
#include <vector>

namespace basalt::wire::auth {

enum class SignatureAlgorithm : std::uint8_t {
    /// parity: `YggdrasilServicesKeyInfo`, what Mojang signs a profile key with.
    Sha1WithRsa,
    /// parity: `ProfilePublicKey.createSignatureValidator`, what a player signs a message with.
    Sha256WithRsa,
};

/// parity: SignatureValidator.from(PublicKey, algorithm): a signature that fails to parse or match is false.
class RsaPublicKey {
public:
    RsaPublicKey(const RsaPublicKey&) = delete;
    RsaPublicKey& operator=(const RsaPublicKey&) = delete;
    RsaPublicKey(RsaPublicKey&&) noexcept;
    RsaPublicKey& operator=(RsaPublicKey&&) noexcept;
    ~RsaPublicKey();

    [[nodiscard]] static std::optional<RsaPublicKey> parseDer(std::span<const std::uint8_t> der);

    [[nodiscard]] bool verify(SignatureAlgorithm algorithm, std::span<const std::uint8_t> payload,
                              std::span<const std::uint8_t> signature) const;

private:
    struct State;

    RsaPublicKey();

    std::unique_ptr<State> state_;
};

/// parity: ProfilePublicKey.Data.signedPayload: profile id, expiry in epoch milliseconds and key.
[[nodiscard]] std::vector<std::uint8_t> profileKeyPayload(const common::Uuid& profileId, std::int64_t expiresAt,
                                                          std::span<const std::uint8_t> publicKeyDer);

/// parity: authlib ServicesKeySet PROFILE_KEY: a set that could not be fetched is empty and validates nothing.
class ServicesKeySet {
public:
    static constexpr std::string_view kUrl = "https://api.minecraftservices.com/publickeys";

    ServicesKeySet();
    ServicesKeySet(ServicesKeySet&&) noexcept;
    ServicesKeySet& operator=(ServicesKeySet&&) noexcept;
    ServicesKeySet(const ServicesKeySet&) = delete;
    ServicesKeySet& operator=(const ServicesKeySet&) = delete;
    ~ServicesKeySet();

    [[nodiscard]] static ServicesKeySet parse(std::string_view json);

    [[nodiscard]] static ServicesKeySet fetch();

    [[nodiscard]] static ServicesKeySet ofProfileKeys(std::vector<RsaPublicKey> keys);

    /// parity: `Services.canValidateProfileKeys`.
    [[nodiscard]] bool canValidateProfileKeys() const noexcept { return !profileKeys_.empty(); }

    /// parity: `SignatureValidator.from(ServicesKeySet, PROFILE_KEY)`: any one key matching is enough.
    [[nodiscard]] bool verifyProfileKey(std::span<const std::uint8_t> payload,
                                        std::span<const std::uint8_t> signature) const;

private:
    std::vector<RsaPublicKey> profileKeys_;
};

} // namespace basalt::wire::auth
