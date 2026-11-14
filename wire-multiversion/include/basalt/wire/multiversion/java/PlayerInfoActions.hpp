#pragma once

#include <basalt/wire/multiversion/ProtocolChain.hpp>

#include <cstdint>

namespace basalt::wire::multiversion::java::playerInfoActions {

/// Removes the actions in `dropped`, and each entry's data for them, from a player info update.
void drop(MutablePacket& packet, std::uint8_t dropped, bool jsonNames = false);

/// Before 1.20.3 a display name in the tab list was a json string rather than an NBT component.
void displayNamesAsJson(MutablePacket& packet);

} // namespace basalt::wire::multiversion::java::playerInfoActions
