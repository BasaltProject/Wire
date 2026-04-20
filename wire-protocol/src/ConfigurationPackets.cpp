#include <basalt/wire/protocol/ProtocolBuffer.hpp>
#include <basalt/wire/protocol/configuration/ConfigurationPackets.hpp>

#include <cstdint>

namespace basalt::wire::protocol::configuration {
namespace {

template <typename Entry, typename Read>
std::vector<Entry> readList(io::ByteBuffer& buffer, std::int32_t bytesPerEntry, Read&& read) {
    const std::int32_t count = readCollectionSize(buffer, bytesPerEntry);
    std::vector<Entry> out;
    out.reserve(static_cast<std::size_t>(count));
    for (std::int32_t i = 0; i < count && !buffer.failed(); ++i) {
        out.push_back(read(buffer));
    }
    return out;
}

template <typename Entry>
void writeList(io::ByteBuffer& buffer, const std::vector<Entry>& entries) {
    varInt::write(buffer, static_cast<std::int32_t>(entries.size()));
    for (const Entry& entry : entries) {
        entry.write(buffer);
    }
}

} // namespace

void ClientInformationPacket::write(io::ByteBuffer& buffer) const {
    writeString(buffer, locale);
    buffer.writeByte(static_cast<std::uint8_t>(viewDistance));
    varInt::write(buffer, chatFlags);
    writeBoolean(buffer, chatColors);
    buffer.writeByte(skinParts);
    varInt::write(buffer, mainHand);
    writeBoolean(buffer, enableTextFiltering);
    writeBoolean(buffer, enableServerListing);
    varInt::write(buffer, particleStatus);
}

std::optional<ClientInformationPacket> ClientInformationPacket::read(io::ByteBuffer& buffer) {
    ClientInformationPacket packet;
    packet.locale = readString(buffer);
    packet.viewDistance = static_cast<std::int8_t>(buffer.readByte());
    packet.chatFlags = varInt::read(buffer);
    packet.chatColors = readBoolean(buffer);
    packet.skinParts = buffer.readByte();
    packet.mainHand = varInt::read(buffer);
    packet.enableTextFiltering = readBoolean(buffer);
    packet.enableServerListing = readBoolean(buffer);
    packet.particleStatus = varInt::read(buffer);
    return buffer.failed() ? std::nullopt : std::optional{std::move(packet)};
}

void KnownPack::write(io::ByteBuffer& buffer) const {
    writeString(buffer, namespaceName);
    writeString(buffer, id);
    writeString(buffer, version);
}

KnownPack KnownPack::read(io::ByteBuffer& buffer) {
    KnownPack pack;
    pack.namespaceName = readString(buffer);
    pack.id = readString(buffer);
    pack.version = readString(buffer);
    return pack;
}

void SelectKnownPacksPacket::write(io::ByteBuffer& buffer) const { writeList(buffer, packs); }

std::optional<SelectKnownPacksPacket> SelectKnownPacksPacket::read(io::ByteBuffer& buffer) {
    SelectKnownPacksPacket packet;
    packet.packs = readList<KnownPack>(buffer, 3, KnownPack::read);
    return buffer.failed() ? std::nullopt : std::optional{std::move(packet)};
}

void RegistryEntry::write(io::ByteBuffer& buffer) const {
    writeString(buffer, key);
    writeBoolean(buffer, value.has_value());
    if (value) {
        writeAnonymousNbt(buffer, *value);
    }
}

RegistryEntry RegistryEntry::read(io::ByteBuffer& buffer) {
    RegistryEntry entry;
    entry.key = readString(buffer);
    if (readBoolean(buffer)) {
        entry.value = readAnonymousNbt(buffer);
    }
    return entry;
}

void RegistryDataPacket::write(io::ByteBuffer& buffer) const {
    writeString(buffer, registryId);
    writeList(buffer, entries);
}

std::optional<RegistryDataPacket> RegistryDataPacket::read(io::ByteBuffer& buffer) {
    RegistryDataPacket packet;
    packet.registryId = readString(buffer);
    packet.entries = readList<RegistryEntry>(buffer, 2, RegistryEntry::read);
    return buffer.failed() ? std::nullopt : std::optional{std::move(packet)};
}

void TagEntry::write(io::ByteBuffer& buffer) const {
    writeString(buffer, name);
    varInt::write(buffer, static_cast<std::int32_t>(ids.size()));
    for (const std::int32_t id : ids) {
        varInt::write(buffer, id);
    }
}

TagEntry TagEntry::read(io::ByteBuffer& buffer) {
    TagEntry entry;
    entry.name = readString(buffer);
    const std::int32_t count = readCollectionSize(buffer);
    entry.ids.reserve(static_cast<std::size_t>(count));
    for (std::int32_t i = 0; i < count && !buffer.failed(); ++i) {
        entry.ids.push_back(varInt::read(buffer));
    }
    return entry;
}

void TagGroup::write(io::ByteBuffer& buffer) const {
    writeString(buffer, registryId);
    writeList(buffer, tags);
}

TagGroup TagGroup::read(io::ByteBuffer& buffer) {
    TagGroup group;
    group.registryId = readString(buffer);
    group.tags = readList<TagEntry>(buffer, 2, TagEntry::read);
    return group;
}

void TagsPacket::write(io::ByteBuffer& buffer) const { writeList(buffer, groups); }

std::optional<TagsPacket> TagsPacket::read(io::ByteBuffer& buffer) {
    TagsPacket packet;
    packet.groups = readList<TagGroup>(buffer, 2, TagGroup::read);
    return buffer.failed() ? std::nullopt : std::optional{std::move(packet)};
}

void BrandPacket::write(io::ByteBuffer& buffer) const {
    writeString(buffer, kChannel);
    writeString(buffer, brand);
}

std::optional<BrandPacket> BrandPacket::read(io::ByteBuffer& buffer) {
    const std::string channel = readString(buffer);
    if (buffer.failed() || channel != kChannel) {
        return std::nullopt;
    }
    BrandPacket packet{readString(buffer)};
    return buffer.failed() ? std::nullopt : std::optional{std::move(packet)};
}

void PostEffectsPacket::write(io::ByteBuffer& buffer) const {
    varInt::write(buffer, static_cast<std::int32_t>(effects.size()));
    for (const std::string& effect : effects) {
        writeString(buffer, effect);
    }
}

std::optional<PostEffectsPacket> PostEffectsPacket::read(io::ByteBuffer& buffer) {
    PostEffectsPacket packet;
    const std::int32_t count = readCollectionSize(buffer);
    for (std::int32_t i = 0; i < count && !buffer.failed(); ++i) {
        packet.effects.push_back(readString(buffer));
    }
    return buffer.failed() ? std::nullopt : std::optional{std::move(packet)};
}

void UpdateEnabledFeaturesPacket::write(io::ByteBuffer& buffer) const {
    varInt::write(buffer, static_cast<std::int32_t>(features.size()));
    for (const std::string& feature : features) {
        writeString(buffer, feature);
    }
}

std::optional<UpdateEnabledFeaturesPacket> UpdateEnabledFeaturesPacket::read(io::ByteBuffer& buffer) {
    UpdateEnabledFeaturesPacket packet;
    const std::int32_t count = readCollectionSize(buffer);
    for (std::int32_t i = 0; i < count && !buffer.failed(); ++i) {
        packet.features.push_back(readString(buffer));
    }
    return buffer.failed() ? std::nullopt : std::optional{std::move(packet)};
}

} // namespace basalt::wire::protocol::configuration
