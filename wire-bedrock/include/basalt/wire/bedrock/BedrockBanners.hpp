#pragma once

#include <cstdint>
#include <string_view>

namespace basalt::wire::bedrock {

/// The short name that edition knows a banner pattern by. A namespace is optional, and a pattern
/// this table does not have comes back as the plain field.
[[nodiscard]] std::string_view bedrockBannerPattern(std::string_view javaPattern);

/// Where a dye sits in the sixteen, which is the number both editions count it as. That edition
/// counts a banner's colours from the other end, so what goes on the wire is fifteen minus this.
[[nodiscard]] std::int32_t dyeIndex(std::string_view name);

} // namespace basalt::wire::bedrock
