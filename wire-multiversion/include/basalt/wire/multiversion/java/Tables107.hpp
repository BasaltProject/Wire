#pragma once

#include <basalt/wire/multiversion/VersionTables.hpp>

namespace basalt::wire::multiversion::java {

/// What protocol 107 calls everything the version above it names differently.
const VersionTables& tables107();

} // namespace basalt::wire::multiversion::java
