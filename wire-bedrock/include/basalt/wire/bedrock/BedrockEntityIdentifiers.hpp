#pragma once

#include <array>
#include <cstdint>
#include <span>

namespace basalt::wire::bedrock {

/// The payload of `available_entity_identifiers`: one NBT tag, exactly as the reference data ships
/// it. Nothing here reads it; the client does.
[[nodiscard]] std::span<const std::uint8_t> entityIdentifiers();

} // namespace basalt::wire::bedrock
