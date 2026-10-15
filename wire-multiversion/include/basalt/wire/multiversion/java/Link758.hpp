#pragma once

#include <basalt/wire/multiversion/java/RenumberingLink.hpp>

#include <cstdint>
#include <span>

namespace basalt::wire::multiversion::java {

class Link758 final : public RenumberingLink {
public:
    using RenumberingLink::RenumberingLink;

    [[nodiscard]] bool clientbound(protocol::ProtocolState state, MutablePacket& packet) override;

    enum class Field : std::uint8_t { Int, Long, Byte, VarInt, String, Nbt, StringArray };

private:
    static void dropDeathLocation(MutablePacket& packet, std::span<const Field> headFields);
};

} // namespace basalt::wire::multiversion::java
