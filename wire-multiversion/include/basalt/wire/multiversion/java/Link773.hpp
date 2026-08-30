#pragma once

#include <basalt/wire/multiversion/java/RenumberingLink.hpp>
#include <basalt/wire/nbt/NbtTag.hpp>

#include <optional>
#include <string>

namespace basalt::wire::multiversion::java {

/// 1.21.11 to 1.21.9.
///
/// The world border takes ticks here and milliseconds there, which is fifty times the number.
///
/// The bigger half is the registries: 1.21.11 gathered what a dimension and a biome are into named
/// attributes, and a client below it knows none of those names and refuses to load anything. So a
/// dimension is sent that version's own definition and a biome's colours go back into its effects.
class Link773 final : public RenumberingLink {
public:
    using RenumberingLink::RenumberingLink;

    [[nodiscard]] bool clientbound(protocol::ProtocolState state, MutablePacket& packet) override;

private:
    /// A dimension and a biome, as the version below the attributes reads them.
    void rewriteRegistry(MutablePacket& packet) const;

    /// That version's own definition of the dimension, by name.
    ///
    /// Not a translation: what a dimension is was rearranged, and half of what the older one needs
    /// (whether it is natural, whether it burns forever) is not in what arrives at all. A dimension
    /// it has never heard of is shown the overworld, which is the one every client has.
    [[nodiscard]] std::optional<nbt::NbtTag> ownDimension(const std::string& name) const;
};

} // namespace basalt::wire::multiversion::java
