#pragma once

#include "World.hpp"

#include <istream>
#include <string_view>

namespace basalt::wire::wasm::trace {

/// A scenario verb an area adds; `rest` is the line after the verb. False reports a line it cannot use.
using ScenarioVerb = bool (*)(World& world, std::string_view rest);

/// Registers `verb` at static initialisation; an area does it from its own Host<Area>.cpp.
struct ScenarioVerbRegistrar {
    ScenarioVerbRegistrar(std::string_view verb, ScenarioVerb run);
};

/// Runs one command per line; returns false at the first line it does not understand.
[[nodiscard]] bool runScenario(World& world, std::istream& input);

} // namespace basalt::wire::wasm::trace
