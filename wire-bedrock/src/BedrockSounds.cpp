#include <basalt/wire/bedrock/BedrockSounds.hpp>

#include <algorithm>

namespace basalt::wire::bedrock {

std::string_view bedrockSoundName(std::string_view javaSound) {
    // The table is written without a namespace, because that is how the reference records it.
    if (javaSound.starts_with("minecraft:")) {
        javaSound.remove_prefix(std::string_view{"minecraft:"}.size());
    }
    const std::span<const SoundName> names = bedrockSoundNames();
    const auto found = std::ranges::lower_bound(names, javaSound, {}, &SoundName::java);
    if (found == names.end() || found->java != javaSound) {
        return {};
    }
    return found->bedrock;
}

} // namespace basalt::wire::bedrock
