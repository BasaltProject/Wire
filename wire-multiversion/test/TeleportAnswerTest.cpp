#include <basalt/wire/multiversion/ProtocolVersions.hpp>
#include <basalt/wire/multiversion/java/JavaTranslators.hpp>
#include <basalt/wire/protocol/VarInt.hpp>
#include <basalt/wire/protocol/play/PlayPackets.hpp>

#include <doctest.h>

#include <optional>
#include <vector>

using namespace basalt::wire;
namespace play = protocol::play;

TEST_CASE("a 1.8 client's move after a teleport reaches the server as one confirmation with its position") {
    const auto built = multiversion::java::javaTranslators::chainFor(multiversion::protocolVersions::kV1_8);
    REQUIRE(built != nullptr);
    play::PlayerPositionPacket teleport;
    teleport.teleportId = 5;
    io::ByteBuffer sent;
    teleport.write(sent);
    built->chain().clientbound(protocol::ProtocolState::Play, play::PlayerPositionPacket::kId, sent.readable(),
                               [](std::int32_t, std::span<const std::uint8_t>) {});

    io::ByteBuffer move;
    move.writeDouble(8.5);
    move.writeDouble(70.0);
    move.writeDouble(-3.5);
    move.writeFloat(0.0F);
    move.writeFloat(0.0F);
    move.writeByte(0);
    std::vector<std::int32_t> ids;
    std::optional<play::AcceptTeleportationPacket> accepted;
    built->chain().serverbound(protocol::ProtocolState::Play,
                               multiversion::java::javaTranslators::packetIdAt(
                                   multiversion::protocolVersions::kV1_8, protocol::ProtocolState::Play, false,
                                   play::ServerboundPositionLookPacket::kId),
                               move.readable(), [&](std::int32_t id, std::span<const std::uint8_t> body) {
                                   ids.push_back(id);
                                   if (id == play::AcceptTeleportationPacket::kId) {
                                       io::ByteBuffer native = io::ByteBuffer::wrap({body.begin(), body.end()});
                                       accepted = play::AcceptTeleportationPacket::read(native);
                                   }
                               });
    REQUIRE(accepted.has_value());
    CHECK(ids.front() == play::AcceptTeleportationPacket::kId);
    CHECK(accepted->teleportId == 5);
    CHECK(accepted->x == 8.5);
    CHECK(accepted->z == -3.5);
}
