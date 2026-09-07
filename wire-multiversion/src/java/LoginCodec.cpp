#include <basalt/wire/multiversion/ProtocolVersions.hpp>
#include <basalt/wire/multiversion/java/CodecAdditions.hpp>
#include <basalt/wire/multiversion/java/LoginCodec.hpp>
#include <basalt/wire/nbt/NbtIO.hpp>

namespace basalt::wire::multiversion::java {

namespace {

void addMonsterSpawnLimits(nbt::NbtCompound& codec) {
    nbt::NbtTag* types = codec.find("minecraft:dimension_type");
    nbt::NbtCompound* registry = types != nullptr ? types->get<nbt::NbtCompound>() : nullptr;
    nbt::NbtTag* values = registry != nullptr ? registry->find("value") : nullptr;
    nbt::NbtList* list = values != nullptr ? values->get<nbt::NbtList>() : nullptr;
    if (list == nullptr) {
        return;
    }
    for (std::size_t index = 0; index < list->size(); ++index) {
        nbt::NbtCompound* entry = (*list)[index].get<nbt::NbtCompound>();
        nbt::NbtTag* element = entry != nullptr ? entry->find("element") : nullptr;
        nbt::NbtCompound* body = element != nullptr ? element->get<nbt::NbtCompound>() : nullptr;
        if (body == nullptr) {
            continue;
        }
        if (!body->contains("monster_spawn_light_level")) {
            body->putInt("monster_spawn_light_level", 11);
        }
        if (!body->contains("monster_spawn_block_light_limit")) {
            body->putInt("monster_spawn_block_light_limit", 0);
        }
    }
}

} // namespace

void completeLoginCodec(std::int32_t backendProtocol, nbt::NbtCompound& codec) {
    if (backendProtocol < protocolVersions::kV1_19) {
        addMonsterSpawnLimits(codec);
    }
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
