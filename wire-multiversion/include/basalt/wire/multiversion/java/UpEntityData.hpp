#pragma once

#include <basalt/wire/multiversion/ComposedTables.hpp>
#include <basalt/wire/multiversion/ProtocolChain.hpp>

#include <cstdint>
#include <unordered_map>
#include <vector>

namespace basalt::wire::multiversion::java {

class UpEntityData final : public PacketContentRewriter {
public:
    UpEntityData(PacketContentRewriter& next, const ComposedTables& tables, std::int32_t backendVersion);

    [[nodiscard]] bool clientbound(protocol::ProtocolState state, MutablePacket& packet) override;

    [[nodiscard]] bool serverbound(protocol::ProtocolState state, MutablePacket& packet) override;

private:
    [[nodiscard]] bool entityData(MutablePacket& packet) const;

    [[nodiscard]] std::int32_t nativeIndex(std::int32_t nativeType, std::int32_t index) const;

    PacketContentRewriter* next_;
    const ComposedTables* tables_;
    std::int32_t backendVersion_;
    std::vector<std::int32_t> serializers_;
    std::unordered_map<std::int32_t, std::int32_t> types_;
};

} // namespace basalt::wire::multiversion::java
