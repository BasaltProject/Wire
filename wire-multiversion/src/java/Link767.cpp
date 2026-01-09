#include <basalt/wire/multiversion/java/Link767.hpp>
#include <basalt/wire/multiversion/java/LoginFields.hpp>
#include <basalt/wire/protocol/ProtocolBuffer.hpp>
#include <basalt/wire/protocol/VarInt.hpp>
#include <basalt/wire/protocol/login/LoginPackets.hpp>
#include <basalt/wire/protocol/play/EntityPackets.hpp>
#include <basalt/wire/protocol/play/InventoryPackets.hpp>
#include <basalt/wire/protocol/play/PlayPackets.hpp>
#include <basalt/wire/protocol/play/PlayerStatePackets.hpp>
#include <basalt/wire/protocol/play/RecipePackets.hpp>
#include <basalt/wire/protocol/play/SimplePackets.hpp>

#include <cmath>
#include <cstdint>

namespace basalt::wire::multiversion::java {
namespace {

namespace play = protocol::play;
using protocol::ProtocolState;

/// 1.21's own numbering for the two packets that were renamed or replaced under it.
constexpr std::int32_t kTeleportEntity767 = 112;
constexpr std::int32_t kSetCarriedItem767 = 83;
constexpr std::int32_t kClientInformationPlay = 14;
constexpr std::int32_t kClientInformationConfiguration = 0x00;
constexpr std::int32_t kParticlesAll = 0;

[[nodiscard]] std::uint8_t angleOf(float degrees) {
    return static_cast<std::uint8_t>(std::lround(degrees * 256.0F / 360.0F));
}

[[nodiscard]] std::size_t varIntSize(std::int32_t value) {
    std::size_t size = 1;
    auto remaining = static_cast<std::uint32_t>(value) >> 7;
    while (remaining != 0) {
        size++;
        remaining >>= 7;
    }
    return size;
}

} // namespace

bool Link767::clientbound(ProtocolState state, MutablePacket& packet) {
    if (state == ProtocolState::Login && packet.nativeId() == protocol::login::LoginFinishedPacket::kId) {
        // 1.21 reads one more flag after the profile. Newer clients behave as if it were on.
        encode(packet, [&](io::ByteBuffer& output) {
            output.writeBytes(packet.body);
            protocol::writeBoolean(output, true);
        });
        // 1.21 calls it game_profile, so the tables see the name as gone and would drop it.
        packet.overrideId(protocol::login::LoginFinishedPacket::kId);
        return true;
    }
    if (state != ProtocolState::Play) {
        return true;
    }
    switch (packet.nativeId()) {
    case play::LoginPlayPacket::kId: {
        // The online-mode flag is already gone: 26.2 to 26.1 took it out.
        const LoginFields login = decode(packet, [](io::ByteBuffer& in) { return LoginFields::read(in, false, true); });
        encode(packet, [&](io::ByteBuffer& out) { login.write(out, false, false); });
        break;
    }
    case play::RespawnPacket::kId: {
        const auto respawn = decode(packet, [](io::ByteBuffer& in) { return play::RespawnPacket::read(in); });
        if (!respawn.has_value()) {
            return true;
        }
        encode(packet, [&](io::ByteBuffer& output) {
            writeSpawnInfoWithoutSeaLevel(output, respawn->spawnInfo);
            output.writeByte(respawn->dataToKeep);
        });
        break;
    }
    case play::PlayerPositionPacket::kId: {
        const auto position = decode(packet, [](io::ByteBuffer& in) { return play::PlayerPositionPacket::read(in); });
        if (!position.has_value()) {
            return true;
        }
        encode(packet, [&](io::ByteBuffer& output) {
            output.writeDouble(position->x);
            output.writeDouble(position->y);
            output.writeDouble(position->z);
            output.writeFloat(position->yaw);
            output.writeFloat(position->pitch);
            // The same bits, in the byte 1.21 keeps them in. The delta movement has nowhere to go: a
            // client that cannot be told about it keeps whatever it had.
            output.writeByte(static_cast<std::uint8_t>(position->relativeFlags));
            protocol::varInt::write(output, position->teleportId);
        });
        break;
    }
    case play::EntityPositionSyncPacket::kId: {
        const auto sync = decode(packet, [](io::ByteBuffer& in) { return play::EntityPositionSyncPacket::read(in); });
        if (!sync.has_value()) {
            return true;
        }
        packet.overrideId(kTeleportEntity767);
        encode(packet, [&](io::ByteBuffer& output) {
            protocol::varInt::write(output, sync->entityId);
            output.writeDouble(sync->x);
            output.writeDouble(sync->y);
            output.writeDouble(sync->z);
            output.writeByte(angleOf(sync->yaw));
            output.writeByte(angleOf(sync->pitch));
            protocol::writeBoolean(output, sync->onGround);
        });
        break;
    }
    case play::EntityTeleportPacket::kId: {
        const auto teleport = decode(packet, [](io::ByteBuffer& in) { return play::EntityTeleportPacket::read(in); });
        if (!teleport.has_value()) {
            return true;
        }
        packet.overrideId(kTeleportEntity767);
        encode(packet, [&](io::ByteBuffer& output) {
            protocol::varInt::write(output, teleport->entityId);
            output.writeDouble(teleport->x);
            output.writeDouble(teleport->y);
            output.writeDouble(teleport->z);
            output.writeByte(angleOf(teleport->yaw));
            output.writeByte(angleOf(teleport->pitch));
            protocol::writeBoolean(output, teleport->onGround);
        });
        break;
    }
    case play::SetHeldSlotPacket::kId:
        packet.overrideId(kSetCarriedItem767);
        break;
    case play::ContainerSetSlotPacket::kId:
    case play::ContainerSetContentPacket::kId:
    case play::ContainerClosePacket::kClientboundId:
    case play::ContainerSetDataPacket::kId:
        narrowContainer(packet);
        break;
    case play::CooldownPacket::kId:
    case play::ExplodePacket::kId:
    case play::UpdateRecipesPacket::kId:
    case play::RecipeBookAddPacket::kId:
    case play::RecipeBookRemovePacket::kId:
        return false;
    default:
        break;
    }
    return true;
}

bool Link767::serverbound(ProtocolState state, MutablePacket& packet) {
    if (state == ProtocolState::Play && (packet.nativeId() == play::ContainerClosePacket::kServerboundId ||
                                         packet.nativeId() == play::ServerboundContainerClickPacket::kId)) {
        widenContainer(packet);
        return true;
    }
    // The client's own settings gained the particle setting in 1.21.2; a 1.21 client stops one field
    // short, so the one this server reads is written in for it.
    const bool clientInformation =
        (state == ProtocolState::Configuration && packet.nativeId() == kClientInformationConfiguration) ||
        (state == ProtocolState::Play && packet.nativeId() == kClientInformationPlay);
    if (clientInformation) {
        encode(packet, [&](io::ByteBuffer& output) {
            output.writeBytes(packet.body);
            protocol::varInt::write(output, kParticlesAll);
        });
    }
    return true;
}

void Link767::narrowContainer(MutablePacket& packet) {
    io::ByteBuffer input = io::ByteBuffer::wrap(packet.body);
    const std::int32_t container = protocol::varInt::read(input);
    encode(packet, [&](io::ByteBuffer& output) {
        output.writeByte(static_cast<std::uint8_t>(container));
        output.writeBytes(input.readable());
    });
}

void Link767::widenContainer(MutablePacket& packet) {
    io::ByteBuffer input = io::ByteBuffer::wrap(packet.body);
    const auto container = static_cast<std::int8_t>(input.readByte());
    encode(packet, [&](io::ByteBuffer& output) {
        protocol::varInt::write(output, container);
        output.writeBytes(input.readable());
    });
}

void Link767::writeSpawnInfoWithoutSeaLevel(io::ByteBuffer& output, const play::SpawnInfo& spawn) {
    io::ByteBuffer whole;
    spawn.write(whole);
    // Walk to the sea level and stop: it is a varint at the very end, so what comes before it is
    // written straight out.
    const std::span<const std::uint8_t> bytes = whole.readable();
    output.writeBytes(bytes.first(bytes.size() - varIntSize(spawn.seaLevel)));
}

} // namespace basalt::wire::multiversion::java
