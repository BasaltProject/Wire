#pragma once

#include <basalt/wire/common/placeholder/Placeholders.hpp>

#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace basalt::wire::common::placeholder {

/// What to put where a placeholder could not be answered, or nothing to keep it as written. Called
/// only on failure.
using UnresolvedHandler = std::function<std::optional<std::string>(std::string_view name, const PlaceholderValue&)>;

/// Replacing `%name%` in operator-written text. A name may not contain whitespace, which keeps
/// prose safe: "50% off, and 20% more" holds no placeholder. `%%` is a literal per cent sign.
///
/// An unanswerable name is left as written by default, since text still saying
/// `%basalt_player_name%` tells the operator what went wrong where a silently dropped word does
/// not. `onUnresolved` lets a caller decide otherwise: that answer is right for a config being
/// checked and wrong for a line strangers read.
[[nodiscard]] std::string substitute(std::string_view text, const PlaceholderRegistry& registry,
                                     const PlaceholderContext& context = PlaceholderContext{},
                                     const UnresolvedHandler& onUnresolved = {});

/// Every placeholder name in `text`, for working out up front what a piece of text will need.
[[nodiscard]] std::vector<std::string> namesIn(std::string_view text);

/// Case-insensitive glob where `*` stands for any run of characters.
///
/// Lives here because both the `matches` operator in a condition and any caller comparing a host or
/// an address needs the same rule, and two implementations of "what does `*` mean" is one too many.
[[nodiscard]] bool globMatches(std::string_view pattern, std::string_view value);

} // namespace basalt::wire::common::placeholder
