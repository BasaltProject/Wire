#include <basalt/wire/common/placeholder/PlaceholderText.hpp>
#include <basalt/wire/config/AnimationPlaceholders.hpp>

#include <algorithm>
#include <format>
#include <string>
#include <utility>

namespace basalt::wire::config {
namespace {

using wire::common::placeholder::PlaceholderValue;

/// Blanks any `%basalt_animation_*%` in a frame, so the substitution below cannot re-enter here.
std::string withoutNestedAnimations(const std::string& frame) {
    std::string result = frame;
    for (const std::string& name : wire::common::placeholder::namesIn(frame)) {
        if (!wire::common::placeholder::lowercaseAscii(name).starts_with(kAnimationIdentifier)) {
            continue;
        }
        const std::string token = "%" + name + "%";
        for (std::size_t at = result.find(token); at != std::string::npos; at = result.find(token, at)) {
            result.erase(at, token.size());
        }
    }
    return result;
}

} // namespace

wire::common::placeholder::PlaceholderProvider animationPlaceholders(AnimationPlaceholderSources sources) {
    return
        [sources = std::move(sources)](
            std::string_view params, const wire::common::placeholder::PlaceholderContext& context) -> PlaceholderValue {
            if (params.empty()) {
                return PlaceholderValue::unknown();
            }
            const std::shared_ptr<const AnimationsConfig> held = sources.animations();
            const Animation* animation = held->get(params);
            if (animation == nullptr) {
                return PlaceholderValue::unavailable(
                    std::format("no animation named '{}' in {}", params, AnimationsConfig::kFileName));
            }
            const std::string& frame = animation->frameAt(sources.nowMillis());
            if (frame.find('%') == std::string::npos) {
                return PlaceholderValue::of(frame);
            }
            return PlaceholderValue::of(
                wire::common::placeholder::substitute(withoutNestedAnimations(frame), sources.registry(), context));
        };
}

wire::common::placeholder::PlaceholderCatalog
animationPlaceholderNames(std::function<std::shared_ptr<const AnimationsConfig>()> animations) {
    return [animations = std::move(animations)] {
        std::vector<std::string> names = animations()->names();
        std::ranges::sort(names);
        return names;
    };
}

} // namespace basalt::wire::config
