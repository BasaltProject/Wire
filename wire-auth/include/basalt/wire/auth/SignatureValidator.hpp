#pragma once

#include <basalt/wire/common/Uuid.hpp>

#include <cstdint>
#include <memory>
#include <optional>
#include <span>
#include <string_view>
#include <vector>

namespace basalt::wire::auth {

/// The two JCA names secure chat verifies with, both RSA with PKCS#1 v1.5 padding.
enum class SignatureAlgorithm : std::uint8_t {
    /// parity: `YggdrasilServicesKeyInfo`, what Mojang signs a profile key with.
    Sha1WithRsa,
    /// parity: `ProfilePublicKey.createSignatureValidator`, what a player signs a message with.
    Sha256WithRsa,
};

/// One RSA public key, parsed once and checked against many signatures.
///
/// parity: `SignatureValidator.from(PublicKey, algorithm)`: a signature that fails to parse or to
/// match is simply false, never an error.
class RsaPublicKey {
public:
    RsaPublicKey(const RsaPublicKey&) = delete;
    RsaPublicKey& operator=(const RsaPublicKey&) = delete;
    RsaPublicKey(RsaPublicKey&&) noexcept;
    RsaPublicKey& operator=(RsaPublicKey&&) noexcept;
    ~RsaPublicKey();

    /// X.509 SubjectPublicKeyInfo DER, which is what `Key.getEncoded` gives and the protocol carries.
    [[nodiscard]] static std::optional<RsaPublicKey> parseDer(std::span<const std::uint8_t> der);

    [[nodiscard]] bool verify(SignatureAlgorithm algorithm, std::span<const std::uint8_t> payload,
                              std::span<const std::uint8_t> signature) const;

private:
    struct State;

    RsaPublicKey();

    std::unique_ptr<State> state_;
};

/// parity: `ProfilePublicKey.Data.signedPayload`: the profile id, the expiry in epoch milliseconds
/// and the key, which is what Mojang's signature covers.
[[nodiscard]] std::vector<std::uint8_t> profileKeyPayload(const common::Uuid& profileId, std::int64_t expiresAt,
                                                          std::span<const std::uint8_t> publicKeyDer);

/// The keys Mojang's services publish, of the one kind secure chat needs.
///
/// parity: authlib's `ServicesKeySet`, `PROFILE_KEY` being the `playerCertificateKeys` list. A set
/// that could not be fetched is empty, and an empty set validates nothing.
class ServicesKeySet {
public:
    static constexpr std::string_view kUrl = "https://api.minecraftservices.com/publickeys";

    ServicesKeySet();
    ServicesKeySet(ServicesKeySet&&) noexcept;
    ServicesKeySet& operator=(ServicesKeySet&&) noexcept;
    ServicesKeySet(const ServicesKeySet&) = delete;
    ServicesKeySet& operator=(const ServicesKeySet&) = delete;
    ~ServicesKeySet();

    /// The `/publickeys` document. Keys that do not parse are skipped.
    [[nodiscard]] static ServicesKeySet parse(std::string_view json);

    /// One blocking request. Empty when the services cannot be reached.
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
