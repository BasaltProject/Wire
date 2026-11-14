#pragma once

#include <cstdint>
#include <vector>

namespace basalt::wire::multiversion {
class ComposedIdMap;
}

namespace basalt::wire::multiversion::java {

/// Renumbers the items of a hashed container click into the server's ids; false if it cannot be read.
[[nodiscard]] bool renumberClickItems(std::vector<std::uint8_t>& body, const ComposedIdMap& items);

} // namespace basalt::wire::multiversion::java
