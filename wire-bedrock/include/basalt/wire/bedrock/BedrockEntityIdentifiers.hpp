#pragma once

#include <array>
#include <cstdint>
#include <span>
#include <string_view>

namespace basalt::wire::bedrock {

/// The payload of `available_entity_identifiers`: one NBT tag, exactly as the reference data ships
/// it. Nothing here reads it; the client does.
[[nodiscard]] std::span<const std::uint8_t> entityIdentifiers();

/// What that edition calls this kind of entity. Nearly every name is the same on both, so this
/// answers what it was given unless the two disagree: a name a client has no entity for is one it
/// draws nothing at all for, and there is no way to tell from the name which of the two it is.
[[nodiscard]] std::string_view bedrockEntityName(std::string_view javaType);

} // namespace basalt::wire::bedrock
