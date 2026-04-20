#pragma once

#include <basalt/wire/common/placeholder/Placeholders.hpp>

#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace basalt::wire::common::placeholder {

using UnresolvedHandler = std::function<std::optional<std::string>(std::string_view name, const PlaceholderValue&)>;

[[nodiscard]] std::string substitute(std::string_view text, const PlaceholderRegistry& registry,
                                     const PlaceholderContext& context = PlaceholderContext{},
                                     const UnresolvedHandler& onUnresolved = {});

[[nodiscard]] std::vector<std::string> namesIn(std::string_view text);

[[nodiscard]] bool globMatches(std::string_view pattern, std::string_view value);

} // namespace basalt::wire::common::placeholder
