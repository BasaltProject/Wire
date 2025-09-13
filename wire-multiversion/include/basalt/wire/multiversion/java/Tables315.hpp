#pragma once

#include <basalt/wire/multiversion/VersionTables.hpp>

namespace basalt::wire::multiversion::java {

/// What protocol 315 calls everything the version above it names differently.
const VersionTables& tables315();

} // namespace basalt::wire::multiversion::java
