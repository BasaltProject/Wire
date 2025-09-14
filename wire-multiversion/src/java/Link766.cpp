#include <basalt/wire/common/Uuid.hpp>
#include <basalt/wire/multiversion/java/Link766.hpp>
#include <basalt/wire/protocol/ProtocolBuffer.hpp>
#include <basalt/wire/protocol/VarInt.hpp>
#include <basalt/wire/protocol/configuration/ConfigurationPackets.hpp>
#include <basalt/wire/protocol/play/EntityPackets.hpp>
#include <basalt/wire/protocol/play/PlayPackets.hpp>

#include <algorithm>
#include <array>
#include <string>
#include <tuple>
#include <vector>

namespace basalt::wire::multiversion::java {
namespace {

namespace play = protocol::play;
using protocol::ProtocolState;

constexpr std::uint64_t kFnvPrime = 0x100000001B3ULL;
constexpr std::uint64_t kHighSeed = 0xCBF29CE484222325ULL;
constexpr std::uint64_t kLowSeed = 0x7ED55D16AA5D1F27ULL;

/// parity: `LevelEvent.SOUND_PLAY_JUKEBOX_SONG`.
constexpr std::int32_t kJukeboxPlay = 1010;
constexpr std::int32_t kMovePlayerRot = 32;

constexpr std::array<std::string_view, 3> kRegistries1_21Added = {
    "minecraft:enchantment",
    "minecraft:painting_variant",
    "minecraft:jukebox_song",
};

[[nodiscard]] std::uint64_t hashOf(std::string_view name, std::uint64_t seed) {
    std::uint64_t value = seed;
    for (const char character : name) {
        value ^= static_cast<std::uint64_t>(static_cast<unsigned char>(character));
        value *= kFnvPrime;
    }
    return value;
}

/// The same name always gives the same uuid, which is the whole of what a client needs.
[[nodiscard]] common::Uuid uuidOf(std::string_view name) {
    return common::Uuid{hashOf(name, kHighSeed), hashOf(name, kLowSeed)};
}

} // namespace

bool Link766::clientbound(ProtocolState state, MutablePacket& packet) {
    if (state == ProtocolState::Configuration &&
        packet.nativeId() == protocol::configuration::RegistryDataPacket::kId) {
        const std::string registry = decode(packet, [](io::ByteBuffer& in) { return protocol::readString(in); });
        return std::ranges::find(kRegistries1_21Added, registry) == kRegistries1_21Added.end();
    }
    if (state == ProtocolState::Play && packet.nativeId() == play::UpdateAttributesPacket::kId) {
        unnameModifiers(packet);
        return true;
    }
    if (state == ProtocolState::Play && packet.nativeId() == play::LevelEventPacket::kId) {
        const auto event = decode(packet, [](io::ByteBuffer& in) { return play::LevelEventPacket::read(in); });
        // The song a jukebox plays is a registry id in 1.21 and an item id before it. There is no way
        // to name it over there, so the record simply does not play.
        if (event.has_value() && event->event == kJukeboxPlay) {
            return false;
        }
    }
    return true;
}

bool Link766::serverbound(ProtocolState state, MutablePacket& packet) {
    if (state != ProtocolState::Play) {
        return true;
    }
    switch (packet.nativeId()) {
    case play::ServerboundPositionLookPacket::kId: {
        const auto move =
            decode(packet, [](io::ByteBuffer& in) { return play::ServerboundPositionLookPacket::read(in); });
        if (move.has_value()) {
            yaw_ = move->yaw;
            pitch_ = move->pitch;
        }
        break;
    }
    case kMovePlayerRot: {
        io::ByteBuffer input = io::ByteBuffer::wrap(packet.body);
        const float yaw = input.readFloat();
        const float pitch = input.readFloat();
        if (!input.failed()) {
            yaw_ = yaw;
            pitch_ = pitch;
        }
        break;
    }
    case play::ServerboundUseItemPacket::kId:
        encode(packet, [&](io::ByteBuffer& output) {
            output.writeBytes(packet.body);
            output.writeFloat(yaw_);
            output.writeFloat(pitch_);
        });
        break;
    default:
        break;
    }
    return true;
}

void Link766::unnameModifiers(MutablePacket& packet) {
    io::ByteBuffer input = io::ByteBuffer::wrap(packet.body);
    const std::int32_t entityId = protocol::varInt::read(input);

    struct Modifier {
        std::string name;
        double amount = 0.0;
        std::uint8_t operation = 0;
    };

    struct Attribute {
        std::int32_t id = 0;
        double base = 0.0;
        std::vector<Modifier> modifiers;
    };

    std::vector<Attribute> attributes;
    const std::int32_t count = protocol::varInt::read(input);
    for (std::int32_t i = 0; i < count && !input.failed(); ++i) {
        Attribute attribute;
        attribute.id = protocol::varInt::read(input);
        attribute.base = input.readDouble();
        const std::int32_t modifiers = protocol::varInt::read(input);
        for (std::int32_t j = 0; j < modifiers && !input.failed(); ++j) {
            Modifier modifier;
            modifier.name = protocol::readString(input);
            modifier.amount = input.readDouble();
            modifier.operation = input.readByte();
            attribute.modifiers.push_back(std::move(modifier));
        }
        attributes.push_back(std::move(attribute));
    }
    if (input.failed()) {
        return;
    }
    encode(packet, [&](io::ByteBuffer& output) {
        protocol::varInt::write(output, entityId);
        protocol::varInt::write(output, static_cast<std::int32_t>(attributes.size()));
        for (const Attribute& attribute : attributes) {
            protocol::varInt::write(output, attribute.id);
            output.writeDouble(attribute.base);
            protocol::varInt::write(output, static_cast<std::int32_t>(attribute.modifiers.size()));
            for (const Modifier& modifier : attribute.modifiers) {
                protocol::writeUuid(output, uuidOf(modifier.name));
                output.writeDouble(modifier.amount);
                output.writeByte(modifier.operation);
            }
        }
    });
}

} // namespace basalt::wire::multiversion::java
