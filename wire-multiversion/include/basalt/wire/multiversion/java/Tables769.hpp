#pragma once

#include <basalt/wire/multiversion/VersionTables.hpp>

namespace basalt::wire::multiversion::java {

/// What protocol 769 calls everything the version above it names differently.
const VersionTables& tables769();

} // namespace basalt::wire::multiversion::java
