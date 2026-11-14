#include <basalt/wire/multiversion/ProtocolVersions.hpp>
#include <basalt/wire/multiversion/java/OlderParticles.hpp>
#include <basalt/wire/protocol/ProtocolBuffer.hpp>
#include <basalt/wire/protocol/VarInt.hpp>

#include <cmath>

namespace basalt::wire::multiversion::java::olderParticles {
namespace {

using protocol::play::ParticleOptions;
namespace versions = protocolVersions;

constexpr float kChannel = 255.0F;

/// From 1.21.2 a colour is one int; before, three floats.
void writeColor(io::ByteBuffer& output, std::int32_t rgb, std::int32_t farEnd) {
    if (farEnd >= versions::kV1_21_2) {
        output.writeInt(rgb);
        return;
    }
    output.writeFloat(static_cast<float>((rgb >> 16) & 0xFF) / kChannel);
    output.writeFloat(static_cast<float>((rgb >> 8) & 0xFF) / kChannel);
    output.writeFloat(static_cast<float>(rgb & 0xFF) / kChannel);
}

void writeVibration(io::ByteBuffer& output, const ParticleOptions::Vibration& vibration,
                    const protocol::play::LevelParticlesPacket& particle, std::int32_t farEnd) {
    const auto* entity = std::get_if<ParticleOptions::Vibration::EntityTarget>(&vibration.target);
    if (farEnd < versions::kV1_19) {
        // 1.17 sent the block the vibration started from, and only the entity it travelled to.
        const protocol::play::BlockPos origin{static_cast<std::int32_t>(std::floor(particle.x)),
                                              static_cast<std::int32_t>(std::floor(particle.y)),
                                              static_cast<std::int32_t>(std::floor(particle.z))};
        output.writeLong(origin.pack());
    }
    if (farEnd >= versions::kV1_20_3) {
        protocol::varInt::write(output, entity != nullptr ? 1 : 0);
    } else {
        protocol::writeString(output, entity != nullptr ? "minecraft:entity" : "minecraft:block");
    }
    if (entity != nullptr) {
        protocol::varInt::write(output, entity->entityId);
        if (farEnd >= versions::kV1_19) {
            output.writeFloat(entity->yOffset);
        }
    } else {
        output.writeLong(std::get<protocol::play::BlockPos>(vibration.target).pack());
    }
    protocol::varInt::write(output, vibration.arrivalTicks);
}

bool writeOptions(io::ByteBuffer& output, const ParticleOptions& options,
                  const protocol::play::LevelParticlesPacket& particle, std::int32_t farEnd,
                  const std::function<std::int32_t(std::int32_t)>& blockState,
                  const std::function<void(io::ByteBuffer&, const ParticleOptions::Item&)>& item) {
    if (const auto* block = options.as<ParticleOptions::BlockState>()) {
        protocol::varInt::write(output, blockState(block->stateId));
    } else if (const auto* dust = options.as<ParticleOptions::Dust>()) {
        writeColor(output, dust->color, farEnd);
        output.writeFloat(dust->scale);
    } else if (const auto* transition = options.as<ParticleOptions::DustTransition>();
               transition != nullptr && farEnd >= versions::kV1_21_2) {
        output.writeInt(transition->fromColor);
        output.writeInt(transition->toColor);
        output.writeFloat(transition->scale);
    } else if (transition != nullptr) {
        writeColor(output, transition->fromColor, farEnd);
        output.writeFloat(transition->scale);
        writeColor(output, transition->toColor, farEnd);
    } else if (const auto* stack = options.as<ParticleOptions::Item>()) {
        item(output, *stack);
    } else if (const auto* number = options.as<ParticleOptions::Number>()) {
        output.writeFloat(number->value);
    } else if (const auto* delay = options.as<ParticleOptions::Delay>()) {
        protocol::varInt::write(output, delay->ticks);
    } else if (const auto* vibration = options.as<ParticleOptions::Vibration>()) {
        writeVibration(output, *vibration, particle, farEnd);
    } else if (const auto* color = options.as<ParticleOptions::Color>()) {
        // An effect's colour came in 1.20.5; before, such a particle carried nothing.
        if (farEnd >= versions::kV1_20_5) {
            output.writeInt(color->color);
        }
    } else if (options.as<ParticleOptions::Spell>() == nullptr) {
        // A trail or a geyser: none had options an older client reads the same way.
        return false;
    }
    return true;
}

} // namespace

std::optional<std::vector<std::uint8_t>>
write(const protocol::play::LevelParticlesPacket& particle, std::int32_t type, std::int32_t farEnd,
      const std::function<std::int32_t(std::int32_t)>& blockState,
      const std::function<void(io::ByteBuffer&, const ParticleOptions::Item&)>& item) {
    io::ByteBuffer output;
    if (farEnd >= versions::kV1_20_5) {
        // 1.20.5 moved the particle to the end, after the count.
        protocol::writeBoolean(output, particle.overrideLimiter);
        if (farEnd >= versions::kV1_21_4) {
            protocol::writeBoolean(output, particle.alwaysShow);
        }
        output.writeDouble(particle.x);
        output.writeDouble(particle.y);
        output.writeDouble(particle.z);
        output.writeFloat(particle.offsetX);
        output.writeFloat(particle.offsetY);
        output.writeFloat(particle.offsetZ);
        output.writeFloat(particle.maxSpeedX);
        output.writeInt(particle.count);
        protocol::varInt::write(output, type);
        if (particle.options.has_value() &&
            !writeOptions(output, *particle.options, particle, farEnd, blockState, item)) {
            return std::nullopt;
        }
        return output.toVector();
    }
    if (farEnd >= versions::kV1_19) {
        protocol::varInt::write(output, type);
    } else {
        output.writeInt(type);
    }
    protocol::writeBoolean(output, particle.overrideLimiter);
    if (farEnd >= versions::kV1_15) {
        output.writeDouble(particle.x);
        output.writeDouble(particle.y);
        output.writeDouble(particle.z);
    } else {
        output.writeFloat(static_cast<float>(particle.x));
        output.writeFloat(static_cast<float>(particle.y));
        output.writeFloat(static_cast<float>(particle.z));
    }
    output.writeFloat(particle.offsetX);
    output.writeFloat(particle.offsetY);
    output.writeFloat(particle.offsetZ);
    output.writeFloat(particle.maxSpeedX);
    output.writeInt(particle.count);
    if (particle.options.has_value() && !writeOptions(output, *particle.options, particle, farEnd, blockState, item)) {
        return std::nullopt;
    }
    return output.toVector();
}

} // namespace basalt::wire::multiversion::java::olderParticles
