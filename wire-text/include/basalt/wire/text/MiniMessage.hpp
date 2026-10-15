#pragma once

#include <basalt/wire/text/Component.hpp>

#include <span>
#include <string_view>

namespace basalt::wire::text::miniMessage {

[[nodiscard]] Component parse(std::string_view input);

[[nodiscard]] Component gradient(std::string_view text, std::span<const TextColor> stops);

} // namespace basalt::wire::text::miniMessage
