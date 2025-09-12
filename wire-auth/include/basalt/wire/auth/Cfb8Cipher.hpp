#pragma once

#include <array>
#include <cstdint>
#include <memory>
#include <optional>
#include <span>

namespace basalt::wire::auth {

/// parity: `Crypt.getCipher`, `AES/CFB8/NoPadding` with the key reused as the IV. Stateful across
/// calls, because the keystream runs continuously for the whole connection in each direction.
///
/// Transforms in place: this sits on the hot path of every packet in both directions, so there is no
/// reason for it to allocate per call. The key schedule is expanded once.
class Cfb8Cipher {
public:
    Cfb8Cipher(const Cfb8Cipher&) = delete;
    Cfb8Cipher& operator=(const Cfb8Cipher&) = delete;
    Cfb8Cipher(Cfb8Cipher&&) noexcept;
    Cfb8Cipher& operator=(Cfb8Cipher&&) noexcept;
    ~Cfb8Cipher();

    /// The key is AES-128, 16 bytes; vanilla reuses it verbatim as the initial shift register.
    [[nodiscard]] static std::optional<Cfb8Cipher> create(std::span<const std::uint8_t> key);

    void encrypt(std::span<std::uint8_t> data) noexcept { update(data, true); }

    void decrypt(std::span<std::uint8_t> data) noexcept { update(data, false); }

private:
    struct KeySchedule;

    Cfb8Cipher();

    void update(std::span<std::uint8_t> data, bool encrypting) noexcept;

    std::unique_ptr<KeySchedule> schedule_;
    std::array<std::uint8_t, 16> register_{};
};

} // namespace basalt::wire::auth
