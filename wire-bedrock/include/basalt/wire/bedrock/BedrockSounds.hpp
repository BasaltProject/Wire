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

/// The name a 1001 and newer client reads a numbered sound event by; empty when there is none.
[[nodiscard]] std::string_view bedrockSoundEventName(std::string_view javaSound);

} // namespace basalt::wire::bedrock
