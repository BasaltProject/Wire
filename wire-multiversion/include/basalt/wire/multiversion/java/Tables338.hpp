#pragma once

#include <basalt/wire/multiversion/VersionTables.hpp>

namespace basalt::wire::multiversion::java {

/// What protocol 338 calls everything the version above it names differently.
const VersionTables& tables338();

} // namespace basalt::wire::multiversion::java
