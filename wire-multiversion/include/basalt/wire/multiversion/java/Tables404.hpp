#pragma once

#include <basalt/wire/multiversion/VersionTables.hpp>

namespace basalt::wire::multiversion::java {

/// What protocol 404 calls everything the version above it names differently.
const VersionTables& tables404();

} // namespace basalt::wire::multiversion::java
