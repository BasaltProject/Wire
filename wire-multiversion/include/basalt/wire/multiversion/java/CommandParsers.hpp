#pragma once

#include <basalt/wire/multiversion/ProtocolChain.hpp>

#include <cstdint>

namespace basalt::wire::multiversion::java::commandParsers {

/// Where the parsers with properties sit in the source version, from `firstRegistry` to `lastRegistry` for names.
struct Layout {
    std::int32_t scoreHolder = 30;
    std::int32_t time = 42;
    std::int32_t firstRegistry = 43;
    std::int32_t lastRegistry = 47;
    /// The source version wrote no property for the time parser; a minimum of zero is added.
    bool timeAddsMinimum = false;
    /// For a source parser that the next version expresses as a registry parser: the registry name it then carries.
    const char* (*addedRegistry)(std::int32_t parser) = nullptr;
};

/// Renumbers the argument parsers of a command tree to the next version's layout.
bool remap(MutablePacket& packet, std::int32_t lastRegistry, std::int32_t (*newId)(std::int32_t));

bool remap(MutablePacket& packet, const Layout& layout, std::int32_t (*newId)(std::int32_t));

} // namespace basalt::wire::multiversion::java::commandParsers
