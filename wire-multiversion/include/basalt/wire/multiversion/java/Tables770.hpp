#pragma once

#include <basalt/wire/multiversion/VersionTables.hpp>

namespace basalt::wire::multiversion::java {

/// What protocol 770 calls everything the version above it names differently.
const VersionTables& tables770();

} // namespace basalt::wire::multiversion::java
