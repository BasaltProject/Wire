#pragma once

#include <array>
#include <cstdint>
#include <memory>
#include <optional>
#include <span>

namespace basalt::wire::auth {

/// parity: Crypt.getCipher, AES/CFB8/NoPadding with the key reused as the IV; stateful across calls.
class Cfb8Cipher {
public:
    Cfb8Cipher(const Cfb8Cipher&) = delete;
    Cfb8Cipher& operator=(const Cfb8Cipher&) = delete;
    Cfb8Cipher(Cfb8Cipher&&) noexcept;
    Cfb8Cipher& operator=(Cfb8Cipher&&) noexcept;
    ~Cfb8Cipher();

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
