#pragma once

#include <basalt/wire/io/ByteBuffer.hpp>
#include <basalt/wire/nbt/NbtTag.hpp>

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace basalt::wire::protocol::configuration {

/// Serverbound, id 0x00: client settings. parity: ServerboundClientInformationPacket.
struct ClientInformationPacket {
    static constexpr std::int32_t kId = 0;

    std::string locale;
    std::int8_t viewDistance = 0;
    std::int32_t chatFlags = 0;
    bool chatColors = true;
    std::uint8_t skinParts = 0;
    std::int32_t mainHand = 1;
    bool enableTextFiltering = false;
    bool enableServerListing = true;
    std::int32_t particleStatus = 0;

    void write(io::ByteBuffer& buffer) const;
    [[nodiscard]] static std::optional<ClientInformationPacket> read(io::ByteBuffer& buffer);

    friend bool operator==(const ClientInformationPacket&, const ClientInformationPacket&) = default;
};

/// One data pack identity: namespace, id and version.
struct KnownPack {
    std::string namespaceName;
    std::string id;
    std::string version;

    void write(io::ByteBuffer& buffer) const;
    [[nodiscard]] static KnownPack read(io::ByteBuffer& buffer);

    friend bool operator==(const KnownPack&, const KnownPack&) = default;
};

/// Clientbound id 0x0f and serverbound id 0x07: negotiates which data packs both ends already know,
/// so the registry data can leave out what the client has bundled.
///
/// Basalt sends an empty list: it has no data packs of its own, so every registry entry it sends
/// has to carry its full value.
struct SelectKnownPacksPacket {
    static constexpr std::int32_t kClientboundId = 0x0f;
    static constexpr std::int32_t kServerboundId = 0x07;

    std::vector<KnownPack> packs;

    void write(io::ByteBuffer& buffer) const;
    [[nodiscard]] static std::optional<SelectKnownPacksPacket> read(io::ByteBuffer& buffer);

    friend bool operator==(const SelectKnownPacksPacket&, const SelectKnownPacksPacket&) = default;
};

/// One registry entry: an identifier and its value, absent to defer to the client's own copy.
struct RegistryEntry {
    std::string key;
    std::optional<nbt::NbtTag> value;

    void write(io::ByteBuffer& buffer) const;
    [[nodiscard]] static RegistryEntry read(io::ByteBuffer& buffer);

    friend bool operator==(const RegistryEntry&, const RegistryEntry&) = default;
};

/// Clientbound, id 0x07: one registry's worth of entries.
struct RegistryDataPacket {
    static constexpr std::int32_t kId = 7;

    std::string registryId;
    std::vector<RegistryEntry> entries;

    void write(io::ByteBuffer& buffer) const;
    [[nodiscard]] static std::optional<RegistryDataPacket> read(io::ByteBuffer& buffer);

    friend bool operator==(const RegistryDataPacket&, const RegistryDataPacket&) = default;
};

/// One named tag: the ids are numeric registry entries, not names.
struct TagEntry {
    std::string name;
    std::vector<std::int32_t> ids;

    void write(io::ByteBuffer& buffer) const;
    [[nodiscard]] static TagEntry read(io::ByteBuffer& buffer);

    friend bool operator==(const TagEntry&, const TagEntry&) = default;
};

/// Every tag defined for one registry.
struct TagGroup {
    std::string registryId;
    std::vector<TagEntry> tags;

    void write(io::ByteBuffer& buffer) const;
    [[nodiscard]] static TagGroup read(io::ByteBuffer& buffer);

    friend bool operator==(const TagGroup&, const TagGroup&) = default;
};

/// Clientbound, id 0x0e: every tag the server defines, grouped by registry.
struct TagsPacket {
    static constexpr std::int32_t kId = 0x0e;

    std::vector<TagGroup> groups;

    void write(io::ByteBuffer& buffer) const;
    [[nodiscard]] static std::optional<TagsPacket> read(io::ByteBuffer& buffer);

    friend bool operator==(const TagsPacket&, const TagsPacket&) = default;
};

/// Both directions, id 0x03: ends configuration. No payload.
inline constexpr std::int32_t kFinishConfigurationId = 3;

/// Both directions, id 0x04 in configuration.
struct KeepAlivePacket {
    static constexpr std::int32_t kId = 4;

    std::int64_t keepAliveId = 0;

    void write(io::ByteBuffer& buffer) const { buffer.writeLong(keepAliveId); }

    [[nodiscard]] static std::optional<KeepAlivePacket> read(io::ByteBuffer& buffer) {
        KeepAlivePacket packet{buffer.readLong()};
        return buffer.failed() ? std::nullopt : std::optional{packet};
    }

    friend bool operator==(const KeepAlivePacket&, const KeepAlivePacket&) = default;
};

/// Clientbound id 0x01 in configuration: a custom payload carrying `minecraft:brand`.
///
/// What the client shows in F3 as `"<brand>" server`. Without it that line reads `"null"`, because
/// the client has nothing to name the server with.
struct BrandPacket {
    static constexpr std::int32_t kId = 1;
    static constexpr std::string_view kChannel = "minecraft:brand";

    std::string brand;

    void write(io::ByteBuffer& buffer) const;
    [[nodiscard]] static std::optional<BrandPacket> read(io::ByteBuffer& buffer);

    friend bool operator==(const BrandPacket&, const BrandPacket&) = default;
};

/// Clientbound in configuration and in play: the post-processing shaders the client should run.
///
/// parity: `ClientboundPostEffectsPacket`, a list of identifiers, empty to clear them.
struct PostEffectsPacket {
    static constexpr std::int32_t kConfigurationId = 0x0a;
    static constexpr std::int32_t kPlayId = 0x53;

    std::vector<std::string> effects;

    void write(io::ByteBuffer& buffer) const;
    [[nodiscard]] static std::optional<PostEffectsPacket> read(io::ByteBuffer& buffer);

    friend bool operator==(const PostEffectsPacket&, const PostEffectsPacket&) = default;
};

} // namespace basalt::wire::protocol::configuration
