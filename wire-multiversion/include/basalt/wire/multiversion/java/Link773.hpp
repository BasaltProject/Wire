#pragma once

#include <basalt/wire/multiversion/java/RenumberingLink.hpp>
#include <basalt/wire/nbt/NbtTag.hpp>

#include <optional>
#include <string>

namespace basalt::wire::multiversion::java {

class Link773 final : public RenumberingLink {
public:
    using RenumberingLink::RenumberingLink;

    [[nodiscard]] bool clientbound(protocol::ProtocolState state, MutablePacket& packet) override;

private:
    void rewriteRegistry(MutablePacket& packet) const;

    [[nodiscard]] std::optional<nbt::NbtTag> ownDimension(const std::string& name) const;
};

} // namespace basalt::wire::multiversion::java
