#pragma once

#include <basalt/wire/multiversion/ProtocolChain.hpp>

namespace basalt::wire::multiversion::java::chatShapes {

/// Chat before 1.19 was one string, a command being one that starts with a slash: rebuilt as 1.19 has them.
[[nodiscard]] bool from1_18(MutablePacket& packet);

/// 1.19.1 added the messages the client had seen and the last one it received.
void to1_19_1(MutablePacket& packet);

/// 1.19.3 made the signature optional and replaced the seen messages with an offset and a bit set.
void to1_19_3(MutablePacket& packet);

/// 1.20.5 split commands in two; an unsigned one carries only its text.
void to1_20_5(MutablePacket& packet);

/// Before 1.20.5 a chat type was the index itself rather than a registry holder one above it.
void chatTypeAsIndex(MutablePacket& packet);

/// 1.21.5 closed chat and signed commands with a checksum of the seen messages, zero meaning none.
void to1_21_5(MutablePacket& packet);

} // namespace basalt::wire::multiversion::java::chatShapes
