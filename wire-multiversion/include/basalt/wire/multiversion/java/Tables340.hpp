#pragma once

#include <basalt/wire/multiversion/VersionTables.hpp>

namespace basalt::wire::multiversion::java {

/// What protocol 340 calls everything the version above it names differently.
const VersionTables& tables340();

} // namespace basalt::wire::multiversion::java
