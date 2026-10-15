#pragma once

#include <basalt/wire/multiversion/java/RenumberingLink.hpp>
#include <basalt/wire/nbt/NbtTag.hpp>

#include <map>
#include <string>

namespace basalt::wire::multiversion::java {

/// Before that the whole description travels again in the packet, so it is copied back out of the codec.
class Link757 final : public RenumberingLink {
public:
    using RenumberingLink::RenumberingLink;

    [[nodiscard]] bool clientbound(protocol::ProtocolState state, MutablePacket& packet) override;

private:
    void remember(const nbt::NbtTag& codec);

    void writeDimension(io::ByteBuffer& output, const std::string& name) const;

    std::map<std::string, nbt::NbtTag> dimensions_;
};

} // namespace basalt::wire::multiversion::java
