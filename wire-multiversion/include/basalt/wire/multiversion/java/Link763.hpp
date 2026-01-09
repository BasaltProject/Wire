#pragma once

#include <basalt/wire/multiversion/java/RenumberingLink.hpp>
#include <basalt/wire/nbt/NbtTag.hpp>

#include <cstdint>
#include <map>
#include <string>
#include <utility>
#include <vector>

namespace basalt::wire::multiversion::java {

/// 1.20.2 to 1.20 and 1.20.1, where there is no configuration phase at all.
///
/// Everything the configuration would have said arrives inside the packet that starts the game, so
/// this link swallows the phase, keeps the registries out of it and puts them back there.
///
/// Lost with the phase: the server brand and the block tags, which this build sends as configuration
/// packets. A client this old joins and plays without them.
class Link763 final : public RenumberingLink {
public:
    using RenumberingLink::RenumberingLink;

    [[nodiscard]] bool clientbound(protocol::ProtocolState state, MutablePacket& packet) override;

private:
    void rememberRegistry(MutablePacket& packet);

    /// The one tag a 1.20.1 client reads its world out of.
    ///
    /// Chat types are in it because that client will not start without them, and this build has no
    /// such registry to send: one entry, named the way vanilla names its own.
    [[nodiscard]] nbt::NbtCompound codec() const;

    /// What the dimension this player spawned in is called, which is a name over there and a number
    /// here.
    [[nodiscard]] std::string dimensionTypeName(std::int32_t index) const;

    std::map<std::string, std::vector<std::pair<std::string, nbt::NbtCompound>>> registries_;
};

} // namespace basalt::wire::multiversion::java
