#pragma once

#include <basalt/wire/multiversion/ComposedTables.hpp>
#include <basalt/wire/multiversion/java/RenumberingLink.hpp>

namespace basalt::wire::multiversion::java {

/// 1.20.5 to 1.20.4, where an item stops being a set of components and goes back to being NBT.
///
/// 1.20.5 replaced the tag an item carried with a list of components. Below it a stack is
/// `present, id, count, nbt`, so every stack this server writes has to be turned back into that.
///
/// What survives: the item, how many, its damage and its name. The rest of what a component can say
/// is dropped rather than guessed at. The attributes go back to being named here too.
class Link765 final : public RenumberingLink {
public:
    Link765(const VersionTables& tables, const ComposedTables& target) : RenumberingLink(tables), target_(&target) {}

    [[nodiscard]] bool clientbound(protocol::ProtocolState state, MutablePacket& packet) override;

    [[nodiscard]] bool serverbound(protocol::ProtocolState state, MutablePacket& packet) override;

private:
    /// An attribute is a number from 1.21 and a name below it, and the name is the far end's.
    void nameAttributes(MutablePacket& packet) const;

    const ComposedTables* target_;
};

} // namespace basalt::wire::multiversion::java
