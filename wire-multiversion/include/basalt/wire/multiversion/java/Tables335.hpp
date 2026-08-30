#pragma once

#include <basalt/wire/multiversion/VersionTables.hpp>

namespace basalt::wire::multiversion::java {

/// What protocol 335 calls everything the version above it names differently.
const VersionTables& tables335();

} // namespace basalt::wire::multiversion::java
