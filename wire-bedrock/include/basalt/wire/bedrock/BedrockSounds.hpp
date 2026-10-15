#pragma once

#include <array>
#include <cstdint>
#include <span>
#include <string_view>

namespace basalt::wire::bedrock {

struct SoundName {
    std::string_view java;
    std::string_view bedrock;
};

[[nodiscard]] std::span<const SoundName> bedrockSoundNames();

[[nodiscard]] std::string_view bedrockSoundName(std::string_view javaSound);

[[nodiscard]] std::int32_t bedrockSoundEvent(std::string_view javaSound, int protocol);

} // namespace basalt::wire::bedrock
