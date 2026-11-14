#include <basalt/wire/multiversion/java/Link762.hpp>
#include <basalt/wire/multiversion/java/NamedBlockEntities.hpp>
#include <basalt/wire/multiversion/java/NamedNbt.hpp>
#include <basalt/wire/protocol/ProtocolBuffer.hpp>
#include <basalt/wire/protocol/VarInt.hpp>
#include <basalt/wire/protocol/play/BlockEntityPackets.hpp>
#include <basalt/wire/protocol/play/PlayPackets.hpp>
#include <basalt/wire/protocol/play/PlayerStatePackets.hpp>
#include <basalt/wire/protocol/play/WorldPackets.hpp>

#include <cstdint>
#include <string>
#include <vector>

namespace basalt::wire::multiversion::java {
namespace {

namespace play = protocol::play;

void writeOneSided(io::ByteBuffer& input, io::ByteBuffer& output) {
    nbt::NbtTag tag = readNamedTag(input);
    if (auto* entity = tag.get<nbt::NbtCompound>()) {
        static_cast<void>(namedBlockEntities::signAsOneSide(*entity));
    }
    writeNamedNbt(output, tag);
}

void copyNamed(io::ByteBuffer& input, io::ByteBuffer& output) {
    if (const std::optional<NamedNbt> tag = readNamedNbt(input)) {
        output.writeBytes(tag->raw);
    }
}

using protocol::ProtocolState;

struct LoginHead {
    std::int32_t entityId = 0;
    std::uint8_t hardcore = 0;
    std::uint8_t gamemode = 0;
    std::uint8_t previousGamemode = 0;
    std::vector<std::string> worlds;
    nbt::NbtTag codec;
};

} // namespace

void Link762::trustChunkEdges(MutablePacket& packet) {
    io::ByteBuffer input = io::ByteBuffer::wrap(packet.body);
    io::ByteBuffer output;
    output.writeInt(input.readInt());
    output.writeInt(input.readInt());
    copyNamed(input, output);
    const std::int32_t sectionBytes = protocol::varInt::read(input);
    protocol::varInt::write(output, sectionBytes);
    output.writeBytes(input.readSpan(static_cast<std::size_t>(sectionBytes < 0 ? 0 : sectionBytes)));
    const std::int32_t blockEntities = protocol::varInt::read(input);
    protocol::varInt::write(output, blockEntities);
    for (std::int32_t i = 0; i < blockEntities && !input.failed(); ++i) {
        output.writeByte(input.readByte());
        output.writeShort(input.readShort());
        protocol::varInt::write(output, protocol::varInt::read(input));
        writeOneSided(input, output);
    }
    // The light that follows opened with whether to trust the edges until 1.20.
    protocol::writeBoolean(output, true);
    output.writeBytes(input.readable());
    if (!input.failed()) {
        packet.body = output.toVector();
    }
}

bool Link762::clientbound(ProtocolState state, MutablePacket& packet) {
    if (state != ProtocolState::Play) {
        return true;
    }
    switch (packet.nativeId()) {
    case play::MapChunkPacket::kId:
        trustChunkEdges(packet);
        break;
    case play::UpdateLightPacket::kId: {
        io::ByteBuffer input = io::ByteBuffer::wrap(packet.body);
        const std::int32_t x = protocol::varInt::read(input);
        const std::int32_t z = protocol::varInt::read(input);
        encode(packet, [&](io::ByteBuffer& output) {
            protocol::varInt::write(output, x);
            protocol::varInt::write(output, z);
            protocol::writeBoolean(output, true);
            output.writeBytes(input.readable());
        });
        break;
    }
    case play::SectionBlocksUpdatePacket::kId: {
        io::ByteBuffer input = io::ByteBuffer::wrap(packet.body);
        const std::int64_t section = input.readLong();
        encode(packet, [&](io::ByteBuffer& output) {
            output.writeLong(section);
            protocol::writeBoolean(output, false);
            output.writeBytes(input.readable());
        });
        break;
    }
    case play::BlockEntityDataPacket::kId:
        rewrite(packet, [](io::ByteBuffer& input, io::ByteBuffer& output) {
            output.writeLong(input.readLong());
            protocol::varInt::write(output, protocol::varInt::read(input));
            writeOneSided(input, output);
        });
        break;
    case play::OpenSignEditorPacket::kId: {
        io::ByteBuffer input = io::ByteBuffer::wrap(packet.body);
        const std::int64_t position = input.readLong();
        encode(packet, [&](io::ByteBuffer& output) { output.writeLong(position); });
        break;
    }
    case play::PlayerCombatKillPacket::kId: {
        io::ByteBuffer input = io::ByteBuffer::wrap(packet.body);
        const std::int32_t playerId = protocol::varInt::read(input);
        encode(packet, [&](io::ByteBuffer& output) {
            protocol::varInt::write(output, playerId);
            output.writeInt(-1);
            output.writeBytes(input.readable());
        });
        break;
    }
    case play::LoginPlayPacket::kId:
        dropPortalCooldown(packet, true);
        break;
    case play::RespawnPacket::kId:
        dropPortalCooldown(packet, false);
        break;
    default:
        break;
    }
    return true;
}

bool Link762::serverbound(ProtocolState state, MutablePacket& packet) {
    if (state != ProtocolState::Play || packet.nativeId() != play::SignUpdatePacket::kId) {
        return true;
    }
    io::ByteBuffer input = io::ByteBuffer::wrap(packet.body);
    const std::int64_t position = input.readLong();
    encode(packet, [&](io::ByteBuffer& output) {
        output.writeLong(position);
        protocol::writeBoolean(output, true);
        output.writeBytes(input.readable());
    });
    return true;
}

void Link762::dropPortalCooldown(MutablePacket& packet, bool afterLogin) {
    io::ByteBuffer input = io::ByteBuffer::wrap(packet.body);
    LoginHead head;
    if (afterLogin) {
        head.entityId = input.readInt();
        head.hardcore = input.readByte();
        head.gamemode = input.readByte();
        head.previousGamemode = input.readByte();
        const std::int32_t worlds = protocol::varInt::read(input);
        for (std::int32_t i = 0; i < worlds && !input.failed(); ++i) {
            head.worlds.push_back(protocol::readString(input));
        }
        head.codec = readNamedTag(input);
    }
    const std::string dimensionType = protocol::readString(input);
    const std::string worldName = protocol::readString(input);
    const std::int64_t seed = input.readLong();
    const std::uint8_t gamemode = afterLogin ? 0 : input.readByte();
    const std::uint8_t previousGamemode = afterLogin ? 0 : input.readByte();
    const std::int32_t maxPlayers = afterLogin ? protocol::varInt::read(input) : 0;
    const std::int32_t viewDistance = afterLogin ? protocol::varInt::read(input) : 0;
    const std::int32_t simulationDistance = afterLogin ? protocol::varInt::read(input) : 0;
    const std::uint8_t reducedDebugInfo = afterLogin ? input.readByte() : 0;
    const std::uint8_t respawnScreen = afterLogin ? input.readByte() : 0;
    const std::uint8_t debug = input.readByte();
    const std::uint8_t flat = input.readByte();
    const std::uint8_t keep = afterLogin ? 0 : input.readByte();
    const bool hasDeath = input.readByte() != 0;
    std::string deathDimension;
    std::int64_t deathPosition = 0;
    if (hasDeath) {
        deathDimension = protocol::readString(input);
        deathPosition = input.readLong();
    }
    static_cast<void>(protocol::varInt::read(input));
    if (input.failed()) {
        return;
    }

    encode(packet, [&](io::ByteBuffer& output) {
        if (afterLogin) {
            output.writeInt(head.entityId);
            output.writeByte(head.hardcore);
            output.writeByte(head.gamemode);
            output.writeByte(head.previousGamemode);
            protocol::varInt::write(output, static_cast<std::int32_t>(head.worlds.size()));
            for (const std::string& world : head.worlds) {
                protocol::writeString(output, world);
            }
            writeNamedNbt(output, head.codec);
        }
        protocol::writeString(output, dimensionType);
        protocol::writeString(output, worldName);
        output.writeLong(seed);
        if (afterLogin) {
            protocol::varInt::write(output, maxPlayers);
            protocol::varInt::write(output, viewDistance);
            protocol::varInt::write(output, simulationDistance);
            output.writeByte(reducedDebugInfo);
            output.writeByte(respawnScreen);
            output.writeByte(debug);
            output.writeByte(flat);
        } else {
            output.writeByte(gamemode);
            output.writeByte(previousGamemode);
            output.writeByte(debug);
            output.writeByte(flat);
            output.writeByte(keep);
        }
        protocol::writeBoolean(output, hasDeath);
        if (hasDeath) {
            protocol::writeString(output, deathDimension);
            output.writeLong(deathPosition);
        }
    });
}

} // namespace basalt::wire::multiversion::java
