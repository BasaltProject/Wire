#include <basalt/wire/multiversion/ProtocolVersions.hpp>
#include <basalt/wire/multiversion/java/EntityDataIndexes.hpp>
#include <basalt/wire/multiversion/java/EntityDataTypes.hpp>
#include <basalt/wire/multiversion/java/UpEntityData.hpp>
#include <basalt/wire/protocol/ProtocolBuffer.hpp>
#include <basalt/wire/protocol/VarInt.hpp>
#include <basalt/wire/protocol/play/EntityData.hpp>
#include <basalt/wire/protocol/play/ParticleOptions.hpp>
#include <basalt/wire/protocol/play/PlayPackets.hpp>
#include <basalt/wire/protocol/play/PlayerStatePackets.hpp>
#include <basalt/wire/protocol/play/RecipePackets.hpp>

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

/// parity: the items a 26.3 server's brewing stand takes as a bottle and as an ingredient, which older servers never
/// sent because their clients had the lists built in.
constexpr std::array<std::int32_t, 3> kBrewingInputs = {1442, 1271, 1445};
constexpr std::array<std::int32_t, 21> kBrewingReagents = {1001, 237,  1274, 1272, 831,  1441, 1403,
                                                           1269, 1279, 1,    1373, 1275, 824,  1064,
                                                           1383, 1193, 973,  1217, 1267, 1273, 1189};

void appendBrewingSets(MutablePacket& packet) {
    io::ByteBuffer input = io::ByteBuffer::wrap(packet.body);
    io::ByteBuffer output;
    const std::int32_t sets = protocol::readCollectionSize(input, 2);
    protocol::varInt::write(output, sets + 2);
    for (std::int32_t i = 0; i < sets && !input.failed(); ++i) {
        protocol::writeString(output, protocol::readString(input));
        const std::int32_t items = protocol::readCollectionSize(input);
        protocol::varInt::write(output, items);
        for (std::int32_t k = 0; k < items && !input.failed(); ++k) {
            protocol::varInt::write(output, protocol::varInt::read(input));
        }
    }
    if (input.failed()) {
        return;
    }
    const auto add = [&output](std::string_view key, std::span<const std::int32_t> items) {
        protocol::writeString(output, key);
        protocol::varInt::write(output, static_cast<std::int32_t>(items.size()));
        for (const std::int32_t item : items) {
            protocol::varInt::write(output, item);
        }
    };
    add("minecraft:brewing_input", kBrewingInputs);
    add("minecraft:brewing_reagent", kBrewingReagents);
    output.writeBytes(input.readable());
    packet.body = output.toVector();
}

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
                types_[entityId] =
                    tables_->nativePlayerEntityType() >= 0 ? play::SpawnEntityPacket::kPlayerEntityType : kUnknownType;
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
    if (!next_->clientbound(state, packet)) {
        return false;
    }
    if (state == protocol::ProtocolState::Play && packet.id == play::UpdateRecipesPacket::kId) {
        appendBrewingSets(packet);
    }
    return true;
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
