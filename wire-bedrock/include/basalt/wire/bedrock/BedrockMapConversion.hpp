#pragma once

#include <basalt/wire/bedrock/BedrockMaps.hpp>
#include <basalt/wire/protocol/play/MapPackets.hpp>

#include <cstdint>
#include <string_view>

namespace basalt::wire::bedrock {

[[nodiscard]] BedrockMapData bedrockMapOf(const protocol::play::MapItemDataPacket& update, std::uint8_t dimension);

/// What a Java map item of that kind shows on Bedrock, for the explorer maps; zero for the others.
[[nodiscard]] std::int32_t bedrockMapExplorerData(std::string_view javaItem);

} // namespace basalt::wire::bedrock
