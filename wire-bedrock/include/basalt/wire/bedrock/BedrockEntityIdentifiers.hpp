#pragma once

#include <array>
#include <cstdint>
#include <span>
#include <string_view>

namespace basalt::wire::bedrock {

[[nodiscard]] std::span<const std::uint8_t> entityIdentifiers();

[[nodiscard]] std::string_view bedrockEntityName(std::string_view javaType);

} // namespace basalt::wire::bedrock
