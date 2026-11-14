#pragma once

#include <basalt/wire/multiversion/ProtocolChain.hpp>
#include <basalt/wire/nbt/NbtTag.hpp>

#include <string>
#include <utility>
#include <vector>

namespace basalt::wire::multiversion::java {

/// 1.20.2 to 1.20.4 took the registries as one codec and named the dimension type; the links hand them 1.20.5's.
class Registries764 final : public PacketContentRewriter {
public:
    explicit Registries764(PacketContentRewriter& next) : next_(&next) {}

    [[nodiscard]] bool clientbound(protocol::ProtocolState state, MutablePacket& packet) override;

    [[nodiscard]] bool serverbound(protocol::ProtocolState state, MutablePacket& packet) override {
        return next_->serverbound(state, packet);
    }

private:
    void remember(const MutablePacket& packet);
    void sendCodec(MutablePacket& packet) const;
    void nameDimension(MutablePacket& packet, bool login) const;
    [[nodiscard]] std::string dimensionTypeName(std::int32_t index) const;

    PacketContentRewriter* next_;
    std::vector<std::pair<std::string, std::vector<std::pair<std::string, nbt::NbtCompound>>>> registries_;
};

} // namespace basalt::wire::multiversion::java
