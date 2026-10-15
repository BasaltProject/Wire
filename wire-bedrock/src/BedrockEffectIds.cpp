
#include <basalt/wire/bedrock/BedrockEffects.hpp>

#include <array>

namespace basalt::wire::bedrock {
namespace {

/// Zero is Bedrock's "no effect", which is where an effect only Java has ends up.
constexpr auto kBedrockEffects = std::to_array<std::int32_t>({
    1,  2,  3,  4, 5,  6, 7, 8,  9,  10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20,
    21, 22, 23, 0, 24, 0, 0, 27, 26, 0,  28, 29, 30, 31, 36, 32, 33, 34, 35, 37,
});

} // namespace

std::int32_t bedrockEffectId(const std::int32_t javaEffect) {
    if (javaEffect < 0 || static_cast<std::size_t>(javaEffect) >= kBedrockEffects.size()) {
        return 0;
    }
    return kBedrockEffects[static_cast<std::size_t>(javaEffect)];
}

} // namespace basalt::wire::bedrock
