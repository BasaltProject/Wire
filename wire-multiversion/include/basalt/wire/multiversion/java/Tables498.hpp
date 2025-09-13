#pragma once

#include <basalt/wire/multiversion/VersionTables.hpp>

namespace basalt::wire::multiversion::java {

/// What protocol 498 calls everything the version above it names differently.
const VersionTables& tables498();

} // namespace basalt::wire::multiversion::java
