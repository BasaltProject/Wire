#pragma once

#include <basalt/wire/multiversion/IdMap.hpp>
#include <basalt/wire/protocol/ProtocolState.hpp>

#include <cstdint>
#include <map>
#include <memory>
#include <string>
#include <vector>

namespace basalt::wire::multiversion {

class PacketIdMap {
public:
    static constexpr std::int32_t kDropped = -1;

    explicit PacketIdMap(std::string encoded) : encoded_(std::move(encoded)) {}

    [[nodiscard]] std::int32_t map(std::int32_t nativeId) const;

    [[nodiscard]] std::int32_t unmap(std::int32_t olderId) const;

    [[nodiscard]] static const PacketIdMap& identity();

    [[nodiscard]] PacketIdMap inverted() const {
        PacketIdMap other{encoded_};
        other.inverted_ = !inverted_;
        return other;
    }

private:
    void parse() const;

    [[nodiscard]] std::int32_t lookupForward(std::int32_t id) const;
    [[nodiscard]] std::int32_t lookupBackward(std::int32_t id) const;

    std::string encoded_;
    bool inverted_ = false;
    mutable std::vector<std::int32_t> forward_;
    mutable std::vector<std::int32_t> backward_;
    mutable bool parsed_ = false;
};

class PacketIdTables {
public:
    PacketIdTables() = default;

    PacketIdTables(std::map<protocol::ProtocolState, PacketIdMap> clientbound,
                   std::map<protocol::ProtocolState, PacketIdMap> serverbound)
        : clientbound_(std::move(clientbound)), serverbound_(std::move(serverbound)) {}

    [[nodiscard]] std::int32_t toOlder(protocol::ProtocolState state, std::int32_t id) const;

    [[nodiscard]] std::int32_t toNative(protocol::ProtocolState state, std::int32_t id) const;

    [[nodiscard]] std::int32_t clientboundToNative(protocol::ProtocolState state, std::int32_t id) const;

    [[nodiscard]] std::int32_t serverboundToOlder(protocol::ProtocolState state, std::int32_t id) const;

    [[nodiscard]] bool hasClientbound(protocol::ProtocolState state) const;

    [[nodiscard]] bool hasServerbound(protocol::ProtocolState state) const;

    [[nodiscard]] PacketIdTables inverted() const;

private:
    std::map<protocol::ProtocolState, PacketIdMap> clientbound_;
    std::map<protocol::ProtocolState, PacketIdMap> serverbound_;
};

struct VersionTables;

[[nodiscard]] VersionTables invertedTables(const VersionTables& down, std::int32_t newer);

struct VersionTables {
    std::int32_t older = 0;
    PacketIdTables packets;
    IdMap blockStates{""};
    IdMap block{"0:2147483647:0|"};
    IdMap item{"0:2147483647:0|"};
    IdMap particleType{"0:2147483647:0|"};
    IdMap entityType{"0:2147483647:0|"};
    IdMap attribute{"0:2147483647:0|"};
    IdMap dataComponentType{"0:2147483647:0|"};
    IdMap menu{"0:2147483647:0|"};
    IdMap mobEffect{"0:2147483647:0|"};
    IdMap soundEvent{"0:2147483647:0|"};
    IdMap gameEvent{"0:2147483647:0|"};
    /// Its order changed in 26.1, and a wrong one is read as another.
    IdMap slotDisplay{"0:2147483647:0|"};
    NameSet newEntries{""};
    NameSet newAttributes{""};
    /// Only the step where the attributes appeared carries them: what a dimension is was rearranged there.
    std::map<std::string, std::string> dimensionTypes;
    std::int32_t playerEntityType = -1;
    /// The same for the version above it, which for the first link of a chain is this build's own.
    std::int32_t newerPlayerEntityType = -1;
    NameIdMap staticBiomes{""};
    NameIdMap attributeNames{""};
    int blockStateDirectBits = 15;
};

} // namespace basalt::wire::multiversion
