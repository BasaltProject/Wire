#pragma once

#include <basalt/wire/multiversion/java/RenumberingLink.hpp>
#include <basalt/wire/nbt/NbtTag.hpp>

#include <cstdint>
#include <map>
#include <span>
#include <string>

namespace basalt::wire::multiversion::java {

class Link758 final : public RenumberingLink {
public:
    using RenumberingLink::RenumberingLink;

    [[nodiscard]] bool clientbound(protocol::ProtocolState state, MutablePacket& packet) override;

    [[nodiscard]] bool serverboundForeign(protocol::ProtocolState state, MutablePacket& packet) override;

    [[nodiscard]] bool serverbound(protocol::ProtocolState state, MutablePacket& packet) override;

    enum class Field : std::uint8_t { Int, Long, Byte, VarInt, String, Nbt, StringArray };

private:
    static void dropDeathLocation(MutablePacket& packet, std::span<const Field> headFields);

    /// Before 1.19 the dimension travels whole again, so it is copied back out of the codec.
    void dimensionInLogin(MutablePacket& packet);
    void dimensionInRespawn(MutablePacket& packet) const;
    void remember(const nbt::NbtTag& codec);
    void writeDimension(io::ByteBuffer& output, const std::string& name) const;

    std::map<std::string, nbt::NbtTag> dimensions_;
};

} // namespace basalt::wire::multiversion::java
