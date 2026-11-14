#include <basalt/wire/multiversion/ProtocolVersions.hpp>
#include <basalt/wire/multiversion/java/Stack765.hpp>
#include <basalt/wire/multiversion/java/Stack769.hpp>
#include <basalt/wire/multiversion/java/UpEntityData.hpp>
#include <basalt/wire/protocol/ProtocolBuffer.hpp>
#include <basalt/wire/protocol/VarInt.hpp>
#include <basalt/wire/protocol/play/ParticleOptions.hpp>

#include <algorithm>
#include <array>

namespace basalt::wire::multiversion::java {
namespace {

namespace play = protocol::play;
using Shape = play::ParticleOptionShape;

constexpr std::int32_t kOpaque = static_cast<std::int32_t>(0xFF000000U);

constexpr std::array<std::pair<std::int32_t, Shape>, 22> kParticleShapes = {{
    {1, Shape::BlockState},      {2, Shape::BlockState},   {7, Shape::Geyser},  {8, Shape::GeyserBase},
    {9, Shape::GeyserBase},      {10, Shape::Geyser},      {15, Shape::Number}, {21, Shape::Dust},
    {22, Shape::DustTransition}, {23, Shape::Spell},       {28, Shape::Color},  {36, Shape::BlockState},
    {46, Shape::Color},          {48, Shape::Number},      {52, Shape::Color},  {56, Shape::Spell},
    {57, Shape::Item},           {58, Shape::Vibration},   {59, Shape::Trail},  {115, Shape::Delay},
    {121, Shape::BlockState},    {125, Shape::BlockState},
}};

constexpr std::int32_t kBlockPosKind = 10;
constexpr std::int32_t kOptionalUuidKind = 13;
constexpr std::int32_t kOptionalGlobalPosKind = 33;
constexpr std::int32_t kVector3Kind = 39;
constexpr std::int32_t kQuaternionKind = 40;

constexpr std::array<std::int32_t, 5> kRawKinds = {kBlockPosKind, kOptionalUuidKind, kOptionalGlobalPosKind,
                                                   kVector3Kind, kQuaternionKind};

} // namespace

std::span<const std::int32_t> UpEntityData::rawKinds() { return kRawKinds; }

play::ParticleOptionShape UpEntityData::nativeShape(std::int32_t particleId) {
    const auto found = std::ranges::find(kParticleShapes, particleId, &std::pair<std::int32_t, Shape>::first);
    return found == kParticleShapes.end() ? Shape::None : found->second;
}

std::optional<std::vector<std::uint8_t>> UpEntityData::readRaw(std::int32_t kind, io::ByteBuffer& in) {
    io::ByteBuffer out;
    const auto optional = [&in, &out] {
        const bool present = protocol::readBoolean(in);
        protocol::writeBoolean(out, present);
        return present;
    };
    switch (kind) {
    case kBlockPosKind:
        out.writeBytes(in.readSpan(8));
        break;
    case kOptionalUuidKind:
        if (optional()) {
            out.writeBytes(in.readSpan(16));
        }
        break;
    case kOptionalGlobalPosKind:
        if (optional()) {
            protocol::writeString(out, protocol::readString(in));
            out.writeBytes(in.readSpan(8));
        }
        break;
    case kVector3Kind:
        out.writeBytes(in.readSpan(12));
        break;
    case kQuaternionKind:
        out.writeBytes(in.readSpan(16));
        break;
    default:
        return std::nullopt;
    }
    return in.failed() ? std::nullopt : std::optional{out.toVector()};
}

std::optional<std::optional<play::WireItemStack>> UpEntityData::olderItem(io::ByteBuffer& in) const {
    io::ByteBuffer components;
    if (backendVersion_ < protocolVersions::kV1_20_5 && !stack765::toComponents(in, components)) {
        return std::nullopt;
    }
    io::ByteBuffer converted;
    const stack769::SourceScope scope(stack769::sourceFor(backendVersion_));
    io::ByteBuffer& source = backendVersion_ < protocolVersions::kV1_20_5 ? components : in;
    if (!stack769::convert(source, converted)) {
        return std::nullopt;
    }
    return play::WireItemStack::read(converted);
}

std::optional<play::SetEntityDataPacket::Value> UpEntityData::olderParticle(io::ByteBuffer& in) const {
    using Options = play::ParticleOptions;
    play::SetEntityDataPacket::ParticleValue particle;
    particle.particleId = protocol::varInt::read(in);
    const Shape shape = nativeShape(tables_->particleType.map(particle.particleId));
    const auto rgb = [&in] {
        std::int32_t color = 0;
        for (int channel = 0; channel < 3; ++channel) {
            color = color << 8 | (static_cast<std::int32_t>(in.readFloat() * 255.0F) & 0xFF);
        }
        return color;
    };
    switch (shape) {
    case Shape::BlockState: {
        const std::int32_t state = tables_->blockStates.map(protocol::varInt::read(in));
        particle.options = Options{Options::BlockState{state == IdMap::kNone ? 0 : state}};
        break;
    }
    case Shape::Dust: {
        const std::int32_t color = rgb();
        particle.options = Options{Options::Dust{color, in.readFloat()}};
        break;
    }
    case Shape::DustTransition: {
        const std::int32_t from = rgb();
        const float scale = in.readFloat();
        particle.options = Options{Options::DustTransition{from, rgb(), scale}};
        break;
    }
    case Shape::Item: {
        const auto stack = olderItem(in);
        if (!stack) {
            return std::nullopt;
        }
        particle.options = Options{Options::Item{.itemId = *stack ? (*stack)->itemId : 0, .count = 1}};
        break;
    }
    case Shape::Color:
    case Shape::Spell:
    case Shape::Trail:
    case Shape::None:
        // parity: before 1.20.5 these had no options; a cloud's colour comes in its own field.
        if (shape == Shape::Color) {
            particle.options = Options{Options::Color{kOpaque}};
        } else if (shape == Shape::Spell) {
            particle.options = Options{Options::Spell{kOpaque, 1.0F}};
        }
        break;
    default:
        particle.options = Options::read(in, shape);
        if (!particle.options) {
            return std::nullopt;
        }
    }
    if (in.failed()) {
        return std::nullopt;
    }
    return particle;
}

play::SetEntityDataPacket::Value UpEntityData::blockState(std::int32_t serializer, std::int32_t state) const {
    const std::int32_t mapped = tables_->blockStates.map(state);
    io::ByteBuffer bytes;
    protocol::varInt::write(bytes, mapped == IdMap::kNone ? 0 : mapped);
    return play::SetEntityDataPacket::RawValue{serializer, bytes.toVector()};
}

} // namespace basalt::wire::multiversion::java
