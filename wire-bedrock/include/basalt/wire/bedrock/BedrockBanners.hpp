#pragma once

#include <cstdint>
#include <string_view>

namespace basalt::wire::bedrock {

[[nodiscard]] std::string_view bedrockBannerPattern(std::string_view javaPattern);

[[nodiscard]] std::int32_t dyeIndex(std::string_view name);

} // namespace basalt::wire::bedrock
