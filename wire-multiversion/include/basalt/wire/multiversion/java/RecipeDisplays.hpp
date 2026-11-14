#pragma once

#include <basalt/wire/multiversion/ComposedTables.hpp>
#include <basalt/wire/multiversion/ProtocolChain.hpp>

namespace basalt::wire::multiversion::java::recipeDisplays {

/// Renumbers the recipe and slot displays of a recipe book addition for a client from 1.21.2 to 1.21.11.
void rewriteBookAdd(MutablePacket& packet, const ComposedTables& tables);

} // namespace basalt::wire::multiversion::java::recipeDisplays
