#pragma once

#include <basalt/wire/multiversion/VersionTables.hpp>

namespace basalt::wire::multiversion::java {

/// What protocol 774 calls everything the version above it names differently.
const VersionTables& tables774();

} // namespace basalt::wire::multiversion::java
