#pragma once

#include <basalt/wire/multiversion/java/RenumberingLink.hpp>
#include <basalt/wire/nbt/NbtTag.hpp>

#include <cstdint>
#include <map>
#include <string>
#include <utility>
#include <vector>

namespace basalt::wire::multiversion::java {

/// Lost with the phase: the server brand and the block tags, which this build sends as configuration packets.
class Link763 final : public RenumberingLink {
public:
    using RenumberingLink::RenumberingLink;

    [[nodiscard]] bool clientbound(protocol::ProtocolState state, MutablePacket& packet) override;

private:
    void rememberRegistry(MutablePacket& packet);

    [[nodiscard]] nbt::NbtCompound codec() const;

    [[nodiscard]] std::string dimensionTypeName(std::int32_t index) const;

    std::map<std::string, std::vector<std::pair<std::string, nbt::NbtCompound>>> registries_;
};

} // namespace basalt::wire::multiversion::java
