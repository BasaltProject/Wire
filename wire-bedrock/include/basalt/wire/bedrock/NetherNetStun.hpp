#pragma once

#include <basalt/wire/io/UdpSocket.hpp>

#include <array>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace basalt::wire::bedrock {

inline constexpr std::size_t kStunHeaderSize = 20;

using StunTransactionId = std::array<std::uint8_t, 12>;

struct StunBindingRequest {
    StunTransactionId transaction{};
    std::string username;
    bool useCandidate = false;
};

/// Demultiplexes one UDP port: a STUN message has zero top bits and carries the magic cookie.
[[nodiscard]] bool looksLikeStun(std::span<const std::uint8_t> datagram) noexcept;

/// The USERNAME of a binding request before it is trusted, which says whose password to check it with.
[[nodiscard]] std::optional<std::string> stunUsername(std::span<const std::uint8_t> datagram);

/// A binding request whose integrity and fingerprint check out against the local ICE password.
[[nodiscard]] std::optional<StunBindingRequest> parseStunBindingRequest(std::span<const std::uint8_t> datagram,
                                                                        std::string_view localPassword);

[[nodiscard]] std::vector<std::uint8_t> buildStunBindingSuccess(const StunTransactionId& transaction,
                                                                const io::UdpEndpoint& mapped,
                                                                std::string_view localPassword);

[[nodiscard]] std::vector<std::uint8_t> buildStunBindingError(const StunTransactionId& transaction, std::uint16_t code,
                                                              std::string_view reason);

} // namespace basalt::wire::bedrock
