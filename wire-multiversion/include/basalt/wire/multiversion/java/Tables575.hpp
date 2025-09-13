#pragma once

#include <basalt/wire/multiversion/VersionTables.hpp>

namespace basalt::wire::multiversion::java {

/// What protocol 575 calls everything the version above it names differently.
const VersionTables& tables575();

} // namespace basalt::wire::multiversion::java
