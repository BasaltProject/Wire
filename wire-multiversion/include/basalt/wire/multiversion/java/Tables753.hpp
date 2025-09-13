#pragma once

#include <basalt/wire/multiversion/VersionTables.hpp>

namespace basalt::wire::multiversion::java {

/// What protocol 753 calls everything the version above it names differently.
const VersionTables& tables753();

} // namespace basalt::wire::multiversion::java
