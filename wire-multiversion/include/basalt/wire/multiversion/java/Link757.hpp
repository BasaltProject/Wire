#pragma once

#include <basalt/wire/multiversion/java/RenumberingLink.hpp>
#include <basalt/wire/nbt/NbtTag.hpp>

#include <map>
#include <string>

namespace basalt::wire::multiversion::java {

/// 1.18.2 to 1.18 and 1.18.1, where the world a player is in is described rather than named.
///
/// 1.18.2 started sending the name of the dimension type and letting the client look it up in the
/// registry it was just sent. Before that the whole description travels again in the packet, so it
/// is copied back out of the codec.
class Link757 final : public RenumberingLink {
public:
    using RenumberingLink::RenumberingLink;

    [[nodiscard]] bool clientbound(protocol::ProtocolState state, MutablePacket& packet) override;

private:
    /// Keeps the description of every dimension the codec carries, by name.
    void remember(const nbt::NbtTag& codec);

    /// The description of that dimension, or an empty one.
    ///
    /// An empty compound rather than nothing at all: a client this old reads a tag here whatever
    /// happens, and one that is merely empty leaves it with defaults instead of a broken stream.
    void writeDimension(io::ByteBuffer& output, const std::string& name) const;

    /// The last codec seen, which is where the description of a dimension comes from.
    std::map<std::string, nbt::NbtTag> dimensions_;
};

} // namespace basalt::wire::multiversion::java
