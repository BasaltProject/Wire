#include <basalt/wire/multiversion/java/Link774.hpp>
#include <basalt/wire/protocol/ProtocolBuffer.hpp>
#include <basalt/wire/protocol/VarInt.hpp>
#include <basalt/wire/protocol/configuration/ConfigurationPackets.hpp>
#include <basalt/wire/protocol/play/EntityPackets.hpp>
#include <basalt/wire/protocol/play/LowPrecisionVec3.hpp>
#include <basalt/wire/protocol/play/WorldPackets.hpp>

#include <algorithm>
#include <cstdint>

namespace basalt::wire::multiversion::java {
namespace {

namespace play = protocol::play;
using protocol::ProtocolState;

constexpr std::string_view kWolfSounds = "minecraft:wolf_sound_variant";

/// parity: `WolfSoundVariant` had no step of its own until the puppies arrived with 26.1.
constexpr std::string_view kStepSound = "step_sound";

/// parity: `ServerboundInteractPacket.ActionType` before 26.1 split the attack out.
constexpr std::int32_t kAttack = 1;
constexpr std::int32_t kInteractAt = 2;

} // namespace

bool Link774::clientbound(ProtocolState state, MutablePacket& packet) {
    if (!RenumberingLink::clientbound(state, packet)) {
        return false;
    }
    if (state == ProtocolState::Configuration &&
        packet.nativeId() == protocol::configuration::RegistryDataPacket::kId) {
        flattenWolfSounds(packet);
        return true;
    }
    if (state == ProtocolState::Play && packet.nativeId() == play::SetTimePacket::kId) {
        rewrite(packet, [](io::ByteBuffer& input, io::ByteBuffer& output) {
            const auto time = play::SetTimePacket::read(input);
            if (!time.has_value()) {
                return;
            }
            const auto clock = std::ranges::find_if(time->clocks, [](const play::ClockUpdate& one) {
                return one.clockId == play::SetTimePacket::kOverworldClock;
            });
            const bool found = clock != time->clocks.end();
            output.writeLong(time->gameTime);
            output.writeLong(found ? clock->totalTicks : time->gameTime);
            protocol::writeBoolean(output, !found || clock->rate != 0.0F);
        });
    }
    return true;
}

bool Link774::serverbound(ProtocolState state, MutablePacket& packet) {
    if (!RenumberingLink::serverbound(state, packet)) {
        return false;
    }
    if (state != ProtocolState::Play || packet.nativeId() != play::InteractPacket::kId) {
        return true;
    }
    rewrite(packet, [&packet](io::ByteBuffer& input, io::ByteBuffer& output) {
        const std::int32_t entityId = protocol::varInt::read(input);
        const std::int32_t action = protocol::varInt::read(input);
        double x = 0.0;
        double y = 0.0;
        double z = 0.0;
        if (action == kInteractAt) {
            x = static_cast<double>(input.readFloat());
            y = static_cast<double>(input.readFloat());
            z = static_cast<double>(input.readFloat());
        }
        const std::int32_t hand = action == kAttack ? play::InteractPacket::kMainHand : protocol::varInt::read(input);
        const bool sneaking = input.readByte() != 0;
        if (action == kAttack) {
            packet.overrideId(play::AttackPacket::kId);
            protocol::varInt::write(output, entityId);
            return;
        }
        protocol::varInt::write(output, entityId);
        protocol::varInt::write(output, hand);
        play::lowPrecisionVec3::write(output, x, y, z);
        protocol::writeBoolean(output, sneaking);
    });
    return true;
}

void Link774::flattenWolfSounds(MutablePacket& packet) {
    auto data =
        decode(packet, [](io::ByteBuffer& in) { return protocol::configuration::RegistryDataPacket::read(in); });
    if (!data.has_value() || data->registryId != kWolfSounds) {
        return;
    }
    for (protocol::configuration::RegistryEntry& entry : data->entries) {
        if (!entry.value.has_value()) {
            continue;
        }
        const nbt::NbtCompound* element = entry.value->get<nbt::NbtCompound>();
        if (element == nullptr) {
            continue;
        }
        const nbt::NbtCompound* adult = element->getCompound("adult_sounds");
        if (adult == nullptr) {
            continue;
        }
        nbt::NbtCompound kept;
        for (std::size_t i = 0; i < adult->size(); ++i) {
            if (adult->names()[i] != kStepSound) {
                kept.set(adult->names()[i], adult->values()[i]);
            }
        }
        entry.value = nbt::NbtTag::ofCompound(std::move(kept));
    }
    encode(packet, [&](io::ByteBuffer& out) { data->write(out); });
}

} // namespace basalt::wire::multiversion::java
