#pragma once

#include <cstdint>
#include <string_view>

namespace basalt::wire::multiversion::java {

[[nodiscard]] std::string_view registryMembersOf(std::int32_t protocol);

[[nodiscard]] bool versionHasMember(std::int32_t protocol, std::string_view registry, std::string_view member);

} // namespace basalt::wire::multiversion::java
