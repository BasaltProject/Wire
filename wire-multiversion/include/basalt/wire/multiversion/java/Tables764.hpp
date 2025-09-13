#pragma once

#include <basalt/wire/multiversion/VersionTables.hpp>

namespace basalt::wire::multiversion::java {

/// What protocol 764 calls everything the version above it names differently.
const VersionTables& tables764();

} // namespace basalt::wire::multiversion::java
