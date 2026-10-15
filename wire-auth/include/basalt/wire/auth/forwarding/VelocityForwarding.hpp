#pragma once

#include <basalt/wire/auth/forwarding/ProxyForwarding.hpp>

#include <cstdint>
#include <expected>
#include <span>
#include <string_view>
#include <vector>

namespace basalt::wire::io {
class ByteBuffer;
}

namespace basalt::wire::auth::forwarding::velocity {

inline constexpr std::string_view kChannel = "velocity:player_info";

inline constexpr std::int32_t kVersionDefault = 1;
inline constexpr std::int32_t kVersionWithKey = 2;
inline constexpr std::int32_t kVersionWithKeyV2 = 3;
inline constexpr std::int32_t kVersionLazySession = 4;

inline constexpr std::int32_t kMaxSupportedVersion = kVersionLazySession;

enum class EncodeError { kInvalidIdentity, kUnsupportedVersion, kCryptoFailed };
[[nodiscard]] std::expected<void, EncodeError>
signInto(io::ByteBuffer& output, std::string_view secret, std::string_view address, const common::Uuid& profileId,
         std::string_view name, const std::vector<SessionProfileProperty>& properties, std::int32_t version);

[[nodiscard]] std::vector<std::uint8_t> request();

[[nodiscard]] ForwardingResult decode(std::span<const std::uint8_t> data, const ProxyForwarding& forwarding);

[[nodiscard]] std::vector<std::uint8_t> sign(std::string_view secret, std::string_view address,
                                             const common::Uuid& profileId, std::string_view name,
                                             const std::vector<SessionProfileProperty>& properties,
                                             std::int32_t version = kMaxSupportedVersion);

} // namespace basalt::wire::auth::forwarding::velocity
