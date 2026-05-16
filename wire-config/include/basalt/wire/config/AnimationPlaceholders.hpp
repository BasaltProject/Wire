#pragma once

#include <basalt/wire/common/placeholder/Placeholders.hpp>
#include <basalt/wire/config/AnimationsConfig.hpp>

#include <cstdint>
#include <functional>
#include <memory>

namespace basalt::wire::config {

struct AnimationPlaceholderSources {
    std::function<std::shared_ptr<const AnimationsConfig>()> animations;
    std::function<const common::placeholder::PlaceholderRegistry&()> registry;
    std::function<std::int64_t()> nowMillis;
};

constexpr std::string_view kAnimationIdentifier = "basalt_animation";

[[nodiscard]] common::placeholder::PlaceholderProvider animationPlaceholders(AnimationPlaceholderSources sources);

[[nodiscard]] common::placeholder::PlaceholderCatalog
animationPlaceholderNames(std::function<std::shared_ptr<const AnimationsConfig>()> animations);

} // namespace basalt::wire::config
