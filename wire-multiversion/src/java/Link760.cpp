#include <basalt/wire/multiversion/java/Link760.hpp>
#include <basalt/wire/protocol/ProtocolBuffer.hpp>
#include <basalt/wire/protocol/VarInt.hpp>
#include <basalt/wire/protocol/play/HudPackets.hpp>

#include <cstdint>

namespace basalt::wire::multiversion::java {
namespace {

namespace play = protocol::play;

constexpr std::int32_t kCustomSound760 = 23;
constexpr std::int32_t kPlayerInfo760 = 55;

/// parity: `ClientboundPlayerInfoPacket.Action`.
constexpr std::int32_t kAddPlayer = 0;
constexpr std::int32_t kUpdateGameMode = 1;
constexpr std::int32_t kUpdateLatency = 2;
constexpr std::int32_t kRemovePlayer = 4;

void writeUuid(io::ByteBuffer& output, const common::Uuid& uuid) {
    output.writeLong(static_cast<std::int64_t>(uuid.high()));
    output.writeLong(static_cast<std::int64_t>(uuid.low()));
}

} // namespace

bool Link760::clientbound(protocol::ProtocolState state, MutablePacket& packet) {
    if (state != protocol::ProtocolState::Play) {
        return true;
    }
    switch (packet.nativeId()) {
    case play::PlayerInfoUpdatePacket::kId: {
        const auto info = decode(packet, [](io::ByteBuffer& in) { return play::PlayerInfoUpdatePacket::read(in); });
        if (!info.has_value()) {
            return true;
        }
        packet.overrideId(kPlayerInfo760);
        return writeOneAction(packet, *info);
    }
    case play::PlayerInfoRemovePacket::kId: {
        const auto remove = decode(packet, [](io::ByteBuffer& in) { return play::PlayerInfoRemovePacket::read(in); });
        if (!remove.has_value()) {
            return true;
        }
        packet.overrideId(kPlayerInfo760);
        encode(packet, [&](io::ByteBuffer& output) {
            protocol::varInt::write(output, kRemovePlayer);
            protocol::varInt::write(output, static_cast<std::int32_t>(remove->players.size()));
            for (const common::Uuid& uuid : remove->players) {
                writeUuid(output, uuid);
            }
        });
        break;
    }
    case play::SoundPacket::kId: {
        // parity: `ClientboundCustomSoundPacket` before 1.19.3 merged it into `ClientboundSoundPacket`.
        io::ByteBuffer input = io::ByteBuffer::wrap(packet.body);
        if (protocol::varInt::read(input) != 0) {
            return false;
        }
        const std::string name = protocol::readString(input);
        if (protocol::readBoolean(input)) {
            static_cast<void>(input.readFloat());
        }
        const std::int32_t source = protocol::varInt::read(input);
        const std::int32_t x = input.readInt();
        const std::int32_t y = input.readInt();
        const std::int32_t z = input.readInt();
        const float volume = input.readFloat();
        const float pitch = input.readFloat();
        const std::int64_t seed = input.readLong();
        if (input.failed()) {
            return true;
        }
        packet.overrideId(kCustomSound760);
        encode(packet, [&](io::ByteBuffer& output) {
            protocol::writeString(output, name);
            protocol::varInt::write(output, source);
            output.writeInt(x);
            output.writeInt(y);
            output.writeInt(z);
            output.writeFloat(volume);
            output.writeFloat(pitch);
            output.writeLong(seed);
        });
        break;
    }
    default:
        break;
    }
    return true;
}

bool Link760::writeOneAction(MutablePacket& packet, const play::PlayerInfoUpdatePacket& info) {
    if ((info.actions & play::PlayerInfoUpdatePacket::kAddPlayer) != 0) {
        encode(packet, [&](io::ByteBuffer& output) {
            protocol::varInt::write(output, kAddPlayer);
            protocol::varInt::write(output, static_cast<std::int32_t>(info.entries.size()));
            for (const play::PlayerInfoEntry& entry : info.entries) {
                writeUuid(output, entry.uuid);
                protocol::writeString(output, entry.name);
                protocol::varInt::write(output, static_cast<std::int32_t>(entry.properties.size()));
                for (const protocol::login::GameProfileProperty& property : entry.properties) {
                    protocol::writeString(output, property.name);
                    protocol::writeString(output, property.value);
                    protocol::writeBoolean(output, property.signature.has_value());
                    if (property.signature.has_value()) {
                        protocol::writeString(output, *property.signature);
                    }
                }
                protocol::varInt::write(output, static_cast<std::int32_t>(entry.gamemode));
                protocol::varInt::write(output, entry.latencyMillis);
                protocol::writeBoolean(output, false);
                protocol::writeBoolean(output, false);
            }
        });
        return true;
    }
    if ((info.actions & play::PlayerInfoUpdatePacket::kUpdateGameMode) != 0) {
        encode(packet, [&](io::ByteBuffer& output) {
            protocol::varInt::write(output, kUpdateGameMode);
            protocol::varInt::write(output, static_cast<std::int32_t>(info.entries.size()));
            for (const play::PlayerInfoEntry& entry : info.entries) {
                writeUuid(output, entry.uuid);
                protocol::varInt::write(output, static_cast<std::int32_t>(entry.gamemode));
            }
        });
        return true;
    }
    if ((info.actions & play::PlayerInfoUpdatePacket::kUpdateLatency) != 0) {
        encode(packet, [&](io::ByteBuffer& output) {
            protocol::varInt::write(output, kUpdateLatency);
            protocol::varInt::write(output, static_cast<std::int32_t>(info.entries.size()));
            for (const play::PlayerInfoEntry& entry : info.entries) {
                writeUuid(output, entry.uuid);
                protocol::varInt::write(output, entry.latencyMillis);
            }
        });
        return true;
    }
    return false;
}

} // namespace basalt::wire::multiversion::java
