#pragma once

#include <cstdint>
#include <string_view>

namespace basalt::wire::multiversion::java::blockEntityTypes {

inline constexpr std::int32_t kNone = -1;

/// The registry name of a block entity type as this build numbers it, or empty.
[[nodiscard]] std::string_view nativeName(std::int32_t nativeId);

/// How a client of `protocol` numbers it, from 1.17 to 1.21.8; `kNone` where that version has no such type.
[[nodiscard]] std::int32_t idAt(std::int32_t protocol, std::int32_t nativeId);

} // namespace basalt::wire::multiversion::java::blockEntityTypes
