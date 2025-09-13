#pragma once

#include <basalt/wire/multiversion/VersionTables.hpp>

namespace basalt::wire::multiversion::java {

/// What protocol 485 calls everything the version above it names differently.
const VersionTables& tables485();

} // namespace basalt::wire::multiversion::java
