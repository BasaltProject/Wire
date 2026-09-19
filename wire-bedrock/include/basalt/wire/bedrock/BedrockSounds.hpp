#pragma once

#include <array>
#include <cstdint>
#include <span>
#include <string_view>

namespace basalt::wire::bedrock {

/// One sound, under both its names.
struct SoundName {
    std::string_view java;
    std::string_view bedrock;
};

/// Every sound this build can name on the other edition, sorted by the Java name.
[[nodiscard]] std::span<const SoundName> bedrockSoundNames();

/// What a Bedrock client calls this sound, or nothing when only Java has a name for it. The
/// namespace is optional: nothing in the table carries one.
[[nodiscard]] std::string_view bedrockSoundName(std::string_view javaSound);

/// The numbered event a Java sound reaches that edition as when it has no name of its own, or
/// minus one when it has neither. parity: Geyser reads the same two fields of the same mapping,
/// a name first and a number after it.
[[nodiscard]] std::int32_t bedrockSoundEvent(std::string_view javaSound, int protocol);

} // namespace basalt::wire::bedrock
