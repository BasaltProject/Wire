#pragma once

#include <cstddef>
#include <cstdint>
#include <string_view>

namespace basalt::wire::protocol::play {

/// The path of an item in the target version's registry, without `minecraft:`; empty for an id it does not have.
[[nodiscard]] std::string_view javaItemName(std::int32_t protocolId);

[[nodiscard]] std::size_t javaItemCount();

} // namespace basalt::wire::protocol::play
