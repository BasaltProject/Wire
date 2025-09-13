#pragma once

#include <basalt/wire/multiversion/IdMap.hpp>
#include <basalt/wire/protocol/ProtocolState.hpp>

#include <cstdint>
#include <map>
#include <memory>
#include <string>
#include <vector>

namespace basalt::wire::multiversion {

/// What one version calls each of another's packets, in one direction of one phase.
///
/// Dense rather than clever: a phase has a couple of hundred packets and the table is indexed by the
/// native id, so translating one is an array read. A dropped packet is one the older version has no
/// word for, thrown away rather than sent as something else.
class PacketIdMap {
public:
    static constexpr std::int32_t kDropped = -1;

    explicit PacketIdMap(std::string encoded) : encoded_(std::move(encoded)) {}

    [[nodiscard]] std::int32_t map(std::int32_t nativeId) const;

    [[nodiscard]] std::int32_t unmap(std::int32_t olderId) const;

    /// Nothing moved, which is the common case and costs nothing to say.
    [[nodiscard]] static const PacketIdMap& identity();

private:
    void parse() const;

    std::string encoded_;
    mutable std::vector<std::int32_t> forward_;
    mutable std::vector<std::int32_t> backward_;
    mutable bool parsed_ = false;
};

/// The packet ids of one version, by phase and direction. An absent map means nothing moved.
class PacketIdTables {
public:
    PacketIdTables() = default;

    PacketIdTables(std::map<protocol::ProtocolState, PacketIdMap> clientbound,
                   std::map<protocol::ProtocolState, PacketIdMap> serverbound)
        : clientbound_(std::move(clientbound)), serverbound_(std::move(serverbound)) {}

    /// Dropped when the older version has no such packet.
    [[nodiscard]] std::int32_t toOlder(protocol::ProtocolState state, std::int32_t id) const;

    [[nodiscard]] std::int32_t toNative(protocol::ProtocolState state, std::int32_t id) const;

    [[nodiscard]] bool hasClientbound(protocol::ProtocolState state) const;

    [[nodiscard]] bool hasServerbound(protocol::ProtocolState state) const;

private:
    std::map<protocol::ProtocolState, PacketIdMap> clientbound_;
    std::map<protocol::ProtocolState, PacketIdMap> serverbound_;
};

/// Everything one step down the chain is: which packet is called what, and how each registry is
/// numbered. All of it is generated from the two versions' own reports, so a link that needs nothing
/// else is a table and no code, which is what most modern links are.
struct VersionTables {
    std::int32_t older = 0;
    PacketIdTables packets;
    IdMap blockStates{""};
    IdMap block{"0:2147483647:0|"};
    /// Below 1.13 the high half of the value is the damage: a variant of an item was a number there
    /// and a name of its own here, and the two travel together so that they map as one thing.
    IdMap item{"0:2147483647:0|"};
    IdMap particleType{"0:2147483647:0|"};
    IdMap entityType{"0:2147483647:0|"};
    IdMap attribute{"0:2147483647:0|"};
    IdMap dataComponentType{"0:2147483647:0|"};
    IdMap menu{"0:2147483647:0|"};
    IdMap mobEffect{"0:2147483647:0|"};
    IdMap soundEvent{"0:2147483647:0|"};
    IdMap gameEvent{"0:2147483647:0|"};
    /// How a recipe draws one slot. Its order changed in 26.1, and a wrong one is read as another.
    IdMap slotDisplay{"0:2147483647:0|"};
    /// Registry entries the version above added, each as its registry and its name. A whole registry
    /// the older version does not have is named with a star in place of the entry.
    NameSet newEntries{""};
    /// Attribute keys the version above invented, taken out of every element that carries them.
    NameSet newAttributes{""};
    /// The older version's own dimension types, as text. Only the step where the attributes appeared
    /// carries them: what a dimension is was rearranged there.
    std::map<std::string, std::string> dimensionTypes;
    /// What the older version numbers `minecraft:player`, or -1 where it is not in its registry.
    std::int32_t playerEntityType = -1;
    /// The same for the version above it, which for the first link of a chain is this build's own.
    std::int32_t newerPlayerEntityType = -1;
    /// What the older version calls its own biomes, for the versions that have them built in.
    NameIdMap staticBiomes{""};
    /// What the older version calls its own attributes, for the versions that read them by name.
    NameIdMap attributeNames{""};
    /// `ceil(log2(states))` on the older side, which is how wide its direct palettes are.
    int blockStateDirectBits = 15;
};

} // namespace basalt::wire::multiversion
