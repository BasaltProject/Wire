#include <basalt/wire/common/SafeInt.hpp>
#include <basalt/wire/multiversion/java/Link767.hpp>
#include <basalt/wire/multiversion/java/LoginFields.hpp>
#include <basalt/wire/multiversion/java/PlayerInfoActions.hpp>
#include <basalt/wire/protocol/ProtocolBuffer.hpp>
#include <basalt/wire/protocol/VarInt.hpp>
#include <basalt/wire/protocol/login/LoginPackets.hpp>
#include <basalt/wire/protocol/play/EntityPackets.hpp>
#include <basalt/wire/protocol/play/InventoryPackets.hpp>
#include <basalt/wire/protocol/play/PlayPackets.hpp>
#include <basalt/wire/protocol/play/PlayerStatePackets.hpp>
#include <basalt/wire/protocol/play/RecipePackets.hpp>
#include <basalt/wire/protocol/play/SimplePackets.hpp>
#include <basalt/wire/protocol/play/WorldPackets.hpp>

#include <cmath>
#include <cstdint>

namespace basalt::wire::multiversion::java {
namespace {

namespace play = protocol::play;

constexpr std::size_t kCursorBytes = 3 * sizeof(float);

/// parity: ServerboundClientTickEndPacket and the four ServerboundMovePlayerPacket kinds, numbered as this server does.
constexpr std::int32_t kClientTickEnd = 13;
constexpr std::int32_t kMovePlayerPos = 30;
constexpr std::int32_t kMovePlayerStatusOnly = 33;
using protocol::ProtocolState;

constexpr std::int32_t kTeleportEntity767 = 112;
constexpr std::int32_t kSetCarriedItem767 = 83;
constexpr std::int32_t kClientInformationPlay = 14;
constexpr std::int32_t kClientInformationConfiguration = 0x00;
constexpr std::int32_t kParticlesAll = 0;

[[nodiscard]] std::uint8_t angleOf(float degrees) {
    return static_cast<std::uint8_t>(common::safeInt(std::round(degrees * 256.0F / 360.0F)));
}

} // namespace

bool Link767::clientbound(ProtocolState state, MutablePacket& packet) {
    if (state == ProtocolState::Login && packet.nativeId() == protocol::login::LoginFinishedPacket::kId) {
        encode(packet, [&](io::ByteBuffer& output) {
            output.writeBytes(packet.body);
            protocol::writeBoolean(output, true);
        });
        packet.overrideId(protocol::login::LoginFinishedPacket::kId);
        return true;
    }
    if (state != ProtocolState::Play) {
        return true;
    }
    switch (packet.nativeId()) {
    case play::PlayerInfoUpdatePacket::kId:
        // 1.21.2 added the order in the tab list.
        playerInfoActions::drop(packet, play::PlayerInfoUpdatePacket::kUpdateListOrder);
        break;
    case play::SetTimePacket::kId: {
        // Before 1.21.2 a stopped clock was a negative time of day, not a flag after it.
        io::ByteBuffer input = io::ByteBuffer::wrap(packet.body);
        const std::int64_t gameTime = input.readLong();
        const std::int64_t dayTime = input.readLong();
        const bool ticking = protocol::readBoolean(input);
        if (input.failed()) {
            return true;
        }
        encode(packet, [&](io::ByteBuffer& output) {
            output.writeLong(gameTime);
            output.writeLong(ticking ? dayTime : (dayTime == 0 ? -1 : -dayTime));
        });
        break;
    }
    case play::LoginPlayPacket::kId: {
        const LoginFields login = decode(packet, [](io::ByteBuffer& in) { return LoginFields::read(in, false, true); });
        encode(packet, [&](io::ByteBuffer& out) { login.write(out, false, false); });
        break;
    }
    case play::RespawnPacket::kId:
        rewrite(packet, [](io::ByteBuffer& input, io::ByteBuffer& output) {
            const LoginFields::SpawnFields spawn = LoginFields::SpawnFields::read(input, true);
            const std::uint8_t dataToKeep = input.readByte();
            spawn.write(output, false);
            output.writeByte(dataToKeep);
        });
        break;
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
            // The delta movement has nowhere to go: a client that cannot be told about it keeps whatever it had.
            output.writeByte(static_cast<std::uint8_t>(position->relativeFlags));
            protocol::varInt::write(output, position->teleportId);
        });
        break;
    }
    case play::EntityPositionSyncPacket::kId:
        packet.overrideId(kTeleportEntity767);
        rewrite(packet, [](io::ByteBuffer& input, io::ByteBuffer& output) {
            protocol::varInt::write(output, protocol::varInt::read(input));
            output.writeBytes(input.readSpan(3 * 8));
            input.skip(3 * 8);
            const float yaw = input.readFloat();
            const float pitch = input.readFloat();
            output.writeByte(angleOf(yaw));
            output.writeByte(angleOf(pitch));
            protocol::writeBoolean(output, input.readByte() != 0);
        });
        break;
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
    if (state == ProtocolState::Play && packet.nativeId() >= kMovePlayerPos &&
        packet.nativeId() <= kMovePlayerStatusOnly) {
        // 1.21.2 ends each client tick with a packet of its own; a client before it moves at most once a tick.
        packet.extras.emplace_back(kClientTickEnd, std::vector<std::uint8_t>{});
    }
    if (state == ProtocolState::Play && packet.nativeId() == play::ServerboundUseItemOnPacket::kId) {
        // 1.21.2 says whether the click landed on the world border, after whether it was inside the block.
        rewrite(packet, [](io::ByteBuffer& input, io::ByteBuffer& output) {
            protocol::varInt::write(output, protocol::varInt::read(input));
            output.writeLong(input.readLong());
            protocol::varInt::write(output, protocol::varInt::read(input));
            output.writeBytes(input.readSpan(kCursorBytes + 1));
            protocol::writeBoolean(output, false);
            output.writeBytes(input.readable());
        });
        return true;
    }
    if (state == ProtocolState::Play && (packet.nativeId() == play::ContainerClosePacket::kServerboundId ||
                                         packet.nativeId() == play::ServerboundContainerClickPacket::kId)) {
        widenContainer(packet);
        return true;
    }
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

} // namespace basalt::wire::multiversion::java
