#pragma once

#include <cstdint>
#include <string_view>

namespace basalt::wire::multiversion::java {

/// What that version's own registry elements are made of: `registry=member` pairs separated by
/// spaces, one and two levels deep. Empty for a version whose data this build does not carry, and
/// then nothing is taken out of an element rather than everything.
[[nodiscard]] std::string_view registryMembersOf(std::int32_t protocol);

/// Whether that version's elements of that registry are ever made of that member.
///
/// False is what costs a client its connection: it reads an element against the shape it was
/// compiled with, and a member it has never seen makes it refuse the registry and stop joining.
[[nodiscard]] bool versionHasMember(std::int32_t protocol, std::string_view registry, std::string_view member);

} // namespace basalt::wire::multiversion::java
