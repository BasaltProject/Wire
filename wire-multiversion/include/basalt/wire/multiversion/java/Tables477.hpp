#pragma once

#include <basalt/wire/multiversion/VersionTables.hpp>

namespace basalt::wire::multiversion::java {

/// What protocol 477 calls everything the version above it names differently.
const VersionTables& tables477();

} // namespace basalt::wire::multiversion::java
