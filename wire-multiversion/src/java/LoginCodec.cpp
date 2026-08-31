#include <basalt/wire/multiversion/ProtocolVersions.hpp>
#include <basalt/wire/multiversion/java/CodecAdditions.hpp>
#include <basalt/wire/multiversion/java/LoginCodec.hpp>
#include <basalt/wire/nbt/NbtIO.hpp>

namespace basalt::wire::multiversion::java {

void completeLoginCodec(std::int32_t backendProtocol, nbt::NbtCompound& codec) {
    if (backendProtocol >= protocolVersions::kV1_19_4) {
        return;
    }
    for (const std::span<const std::uint8_t> blob : registriesAddedIn1_19_4()) {
        const auto parsed = nbt::nbtIO::readAnonymous(blob);
        const nbt::NbtCompound* added = parsed ? parsed->tag.get<nbt::NbtCompound>() : nullptr;
        if (added == nullptr) {
            continue;
        }
        for (std::size_t index = 0; index < added->size(); ++index) {
            const std::string& key = added->names()[index];
            const bool older = key == "minecraft:chat_type" && backendProtocol < protocolVersions::kV1_19_3;
            if (older || !codec.contains(key)) {
                codec.set(key, added->values()[index]);
            }
        }
    }
}

} // namespace basalt::wire::multiversion::java
