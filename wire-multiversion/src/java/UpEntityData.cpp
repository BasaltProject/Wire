#include <basalt/wire/multiversion/ProtocolVersions.hpp>
#include <basalt/wire/multiversion/java/EntityDataIndexes.hpp>
#include <basalt/wire/multiversion/java/EntityDataTypes.hpp>
#include <basalt/wire/multiversion/java/UpEntityData.hpp>
#include <basalt/wire/protocol/VarInt.hpp>
#include <basalt/wire/protocol/play/EntityData.hpp>
#include <basalt/wire/protocol/play/ParticleOptions.hpp>
#include <basalt/wire/protocol/play/PlayPackets.hpp>
#include <basalt/wire/protocol/play/PlayerStatePackets.hpp>

#include <algorithm>
#include <array>

namespace basalt::wire::multiversion::java {
namespace {

namespace play = protocol::play;
using Shape = play::ParticleOptionShape;

constexpr std::int32_t kUnknownType = -1;

constexpr std::array<std::pair<std::int32_t, Shape>, 22> kParticleShapes = {{
    {1, Shape::BlockState},      {2, Shape::BlockState},   {7, Shape::Geyser},  {8, Shape::GeyserBase},
    {9, Shape::GeyserBase},      {10, Shape::Geyser},      {15, Shape::Number}, {21, Shape::Dust},
    {22, Shape::DustTransition}, {23, Shape::Spell},       {28, Shape::Color},  {36, Shape::BlockState},
    {46, Shape::Color},          {48, Shape::Number},      {52, Shape::Color},  {56, Shape::Spell},
    {57, Shape::Item},           {58, Shape::Vibration},   {59, Shape::Trail},  {115, Shape::Delay},
    {121, Shape::BlockState},    {125, Shape::BlockState},
}};

} // namespace

UpEntityData::UpEntityData(PacketContentRewriter& next, const ComposedTables& tables, std::int32_t backendVersion)
    : next_(&next), tables_(&tables), backendVersion_(backendVersion) {
    const std::span<const std::int32_t> older = entityDataTypes(backendVersion);
    if (older.empty()) {
        return;
    }
    const std::int32_t highest = *std::ranges::max_element(older);
    serializers_.assign(static_cast<std::size_t>(highest + 1), -1);
    for (std::size_t kind = 0; kind < older.size(); ++kind) {
        if (older[kind] >= 0) {
            serializers_[static_cast<std::size_t>(older[kind])] = static_cast<std::int32_t>(kind);
        }
    }
}

bool UpEntityData::clientbound(protocol::ProtocolState state, MutablePacket& packet) {
    packet.becomes(packet.id);
    if (state == protocol::ProtocolState::Play) {
        if (packet.id == play::LoginPlayPacket::kId) {
            io::ByteBuffer input = io::ByteBuffer::wrap(packet.body);
            const std::int32_t entityId = input.readInt();
            if (!input.failed()) {
                types_[entityId] = tables_->nativePlayerEntityType() >= 0 ? play::SpawnEntityPacket::kPlayerEntityType
                                                                          : kUnknownType;
            }
        } else if (packet.id == play::SpawnEntityPacket::kId) {
            io::ByteBuffer input = io::ByteBuffer::wrap(packet.body);
            const std::int32_t entityId = protocol::varInt::read(input);
            static_cast<void>(input.readSpan(16));
            const std::int32_t type = protocol::varInt::read(input);
            if (!input.failed()) {
                const std::int32_t mapped = tables_->entityType.map(type);
                types_[entityId] = mapped == IdMap::kNone ? kUnknownType : mapped;
            }
        } else if (packet.id == play::RemoveEntitiesPacket::kId) {
            io::ByteBuffer input = io::ByteBuffer::wrap(packet.body);
            if (const auto gone = play::RemoveEntitiesPacket::read(input)) {
                for (const std::int32_t entityId : gone->entityIds) {
                    types_.erase(entityId);
                }
            }
        } else if (packet.id == play::SetEntityDataPacket::kId && !entityData(packet)) {
            return false;
        }
    }
    return next_->clientbound(state, packet);
}

bool UpEntityData::serverbound(protocol::ProtocolState state, MutablePacket& packet) {
    return next_->serverbound(state, packet);
}

std::int32_t UpEntityData::nativeIndex(std::int32_t nativeType, std::int32_t index) const {
    if (nativeType < 0) {
        return index;
    }
    // The links drop an index and close the gap; climbing opens the gap again, newest rule first.
    for (std::int32_t step = backendVersion_; step < protocolVersions::kNative; ++step) {
        const std::span<const EntityDataIndexRule> rules = entityDataIndexRules(step);
        for (auto rule = rules.rbegin(); rule != rules.rend(); ++rule) {
            if (std::ranges::find(rule->types, nativeType) == rule->types.end()) {
                continue;
            }
            if (rule->shift && index >= rule->index) {
                index++;
            }
        }
    }
    return index;
}

bool UpEntityData::entityData(MutablePacket& packet) const {
    if (serializers_.empty()) {
        return true;
    }
    io::ByteBuffer input = io::ByteBuffer::wrap(packet.body);
    const auto shapeOf = [this](std::int32_t particleId) -> std::optional<Shape> {
        const std::int32_t mapped = tables_->particleType.map(particleId);
        const auto found = std::ranges::find(kParticleShapes, mapped, &std::pair<std::int32_t, Shape>::first);
        return found == kParticleShapes.end() ? Shape::None : found->second;
    };
    auto data = play::SetEntityDataPacket::read(input, shapeOf, serializers_);
    if (!data.has_value()) {
        return false;
    }
    const auto known = types_.find(data->entityId);
    const std::int32_t type = known == types_.end() ? kUnknownType : known->second;
    for (play::SetEntityDataPacket::Entry& entry : data->entries) {
        entry.index = static_cast<std::uint8_t>(nativeIndex(type, entry.index));
    }
    io::ByteBuffer output;
    data->write(output);
    packet.body = output.toVector();
    return true;
}

} // namespace basalt::wire::multiversion::java
