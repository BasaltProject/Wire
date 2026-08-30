#include <basalt/wire/multiversion/ProtocolVersions.hpp>
#include <basalt/wire/multiversion/java/ContentRewriter.hpp>
#include <basalt/wire/protocol/ProtocolBuffer.hpp>
#include <basalt/wire/protocol/VarInt.hpp>
#include <basalt/wire/protocol/configuration/ConfigurationPackets.hpp>
#include <basalt/wire/protocol/play/CommandPackets.hpp>
#include <basalt/wire/protocol/play/EntityData.hpp>
#include <basalt/wire/protocol/play/EntityPackets.hpp>
#include <basalt/wire/protocol/play/ItemComponent.hpp>
#include <basalt/wire/protocol/play/PlayerStatePackets.hpp>
#include <basalt/wire/protocol/play/RecipePackets.hpp>
#include <basalt/wire/protocol/play/SimplePackets.hpp>
#include <basalt/wire/protocol/play/WorldPackets.hpp>

#include <algorithm>
#include <utility>

namespace basalt::wire::multiversion::java {
namespace {

using protocol::ProtocolState;
namespace play = protocol::play;

/// Reads the packet's body without changing it.
template <typename Read>
auto decode(const MutablePacket& packet, Read&& read) {
    io::ByteBuffer input = io::ByteBuffer::wrap(packet.body);
    return std::forward<Read>(read)(input);
}

/// Replaces the packet's body with whatever `write` puts in the buffer.
template <typename Write>
void encode(MutablePacket& packet, Write&& write) {
    io::ByteBuffer output;
    std::forward<Write>(write)(output);
    packet.body = output.toVector();
}

/// An entity nothing announced the type of: only its shared fields can be placed.
constexpr std::int32_t kUnknownType = -1;

} // namespace

ContentRewriter::ContentRewriter(const ComposedTables& tables, const SubstitutionLabels* labels, std::int32_t farEnd,
                                 std::vector<std::int32_t> links)
    : tables_(&tables), labels_(labels),
      entityData_(farEnd, std::move(links),
                  [this](const std::optional<play::WireItemStack>& item) { return stack(item); }),
      chunks_(tables.blockStates, ChunkRewriter::Options{.olderDirectBits = tables.blockStateDirectBits(),
                                                         .fluidCount = tables.fluidCountInSections()}) {}

bool ContentRewriter::clientbound(ProtocolState state, MutablePacket& packet) {
    if (state == ProtocolState::Configuration && packet.nativeId() == protocol::configuration::TagsPacket::kId) {
        return rewriteTags(packet);
    }
    if (state != ProtocolState::Play) {
        return true;
    }
    if (packet.nativeId() == play::CommandsPacket::kId) {
        return rewriteCommands(packet);
    }
    switch (packet.nativeId()) {
    case play::MapChunkPacket::kId: {
        auto chunk = decode(packet, [](io::ByteBuffer& in) { return play::MapChunkPacket::read(in); });
        if (!chunk.has_value()) {
            return true;
        }
        chunk->chunkData = chunks_.rewrite(chunk->chunkData);
        encode(packet, [&](io::ByteBuffer& out) { chunk->write(out); });
        break;
    }
    case play::ClientboundBlockUpdatePacket::kId: {
        auto update = decode(packet, [](io::ByteBuffer& in) { return play::ClientboundBlockUpdatePacket::read(in); });
        if (!update.has_value()) {
            return true;
        }
        update->blockStateId = blockState(update->blockStateId);
        encode(packet, [&](io::ByteBuffer& out) { update->write(out); });
        break;
    }
    case play::SectionBlocksUpdatePacket::kId: {
        auto update = decode(packet, [](io::ByteBuffer& in) { return play::SectionBlocksUpdatePacket::read(in); });
        if (!update.has_value()) {
            return true;
        }
        for (play::SectionBlocksUpdatePacket::Change& change : update->changes) {
            change.blockStateId = blockState(change.blockStateId);
        }
        encode(packet, [&](io::ByteBuffer& out) { update->write(out); });
        break;
    }
    case play::LevelEventPacket::kId: {
        auto event = decode(packet, [](io::ByteBuffer& in) { return play::LevelEventPacket::read(in); });
        if (!event.has_value() || event->event != play::LevelEventPacket::kParticlesDestroyBlock) {
            return true;
        }
        event->data = blockState(event->data);
        encode(packet, [&](io::ByteBuffer& out) { event->write(out); });
        break;
    }
    case play::LevelParticlesPacket::kId: {
        // Only the leading type id, which is all this has business touching: which options a type
        // carries is the registry's business and is not on the wire, so decoding the rest would mean
        // guessing at a shape to write straight back out.
        io::ByteBuffer input = io::ByteBuffer::wrap(packet.body);
        const std::optional<std::int32_t> nativeType = protocol::varInt::read(input);
        if (!nativeType.has_value()) {
            return true;
        }
        const std::int32_t mapped = tables_->particleType.map(*nativeType);
        if (mapped == IdMap::kNone) {
            return false;
        }
        encode(packet, [&](io::ByteBuffer& out) {
            protocol::varInt::write(out, mapped);
            out.writeBytes(input.readable());
        });
        break;
    }
    case play::ExplodePacket::kId: {
        auto explode = decode(packet, [](io::ByteBuffer& in) { return play::ExplodePacket::read(in); });
        if (!explode.has_value()) {
            return true;
        }
        const std::int32_t particle = tables_->particleType.map(explode->particleId);
        if (particle == IdMap::kNone) {
            return false;
        }
        explode->particleId = particle;
        std::vector<play::ExplosionParticle> thrown;
        thrown.reserve(explode->blockParticles.size());
        for (play::ExplosionParticle one : explode->blockParticles) {
            const std::int32_t id = tables_->particleType.map(one.particleId);
            if (id == IdMap::kNone) {
                continue;
            }
            one.particleId = id;
            thrown.push_back(one);
        }
        explode->blockParticles = std::move(thrown);
        encode(packet, [&](io::ByteBuffer& out) { explode->write(out); });
        break;
    }
    case play::SpawnEntityPacket::kId:
        return rewriteSpawn(packet);
    case play::RemoveEntitiesPacket::kId: {
        if (!entityTypes_.empty()) {
            const auto gone = decode(packet, [](io::ByteBuffer& in) { return play::RemoveEntitiesPacket::read(in); });
            if (gone.has_value()) {
                for (const std::int32_t entityId : gone->entityIds) {
                    entityTypes_.erase(entityId);
                }
            }
        }
        break;
    }
    case play::UpdateRecipesPacket::kId:
        return rewriteRecipes(packet);
    case play::UpdateAttributesPacket::kId: {
        auto attributes = decode(packet, [](io::ByteBuffer& in) { return play::UpdateAttributesPacket::read(in); });
        if (!attributes.has_value()) {
            return true;
        }
        std::vector<play::UpdateAttributesPacket::AttributeSnapshot> kept;
        kept.reserve(attributes->attributes.size());
        for (play::UpdateAttributesPacket::AttributeSnapshot snapshot : attributes->attributes) {
            const std::int32_t id = tables_->attribute.map(snapshot.attribute);
            if (id == IdMap::kNone) {
                continue;
            }
            snapshot.attribute = id;
            kept.push_back(std::move(snapshot));
        }
        attributes->attributes = std::move(kept);
        encode(packet, [&](io::ByteBuffer& out) { attributes->write(out); });
        break;
    }
    case play::UpdateMobEffectPacket::kId: {
        auto effect = decode(packet, [](io::ByteBuffer& in) { return play::UpdateMobEffectPacket::read(in); });
        if (!effect.has_value()) {
            return true;
        }
        const std::int32_t id = tables_->mobEffect.map(effect->effect);
        if (id == IdMap::kNone) {
            return false;
        }
        effect->effect = id;
        encode(packet, [&](io::ByteBuffer& out) { effect->write(out); });
        break;
    }
    case play::RemoveMobEffectPacket::kId: {
        auto effect = decode(packet, [](io::ByteBuffer& in) { return play::RemoveMobEffectPacket::read(in); });
        if (!effect.has_value()) {
            return true;
        }
        const std::int32_t id = tables_->mobEffect.map(effect->effect);
        if (id == IdMap::kNone) {
            return false;
        }
        effect->effect = id;
        encode(packet, [&](io::ByteBuffer& out) { effect->write(out); });
        break;
    }
    case play::OpenScreenPacket::kId: {
        auto screen = decode(packet, [](io::ByteBuffer& in) { return play::OpenScreenPacket::read(in); });
        if (!screen.has_value()) {
            return true;
        }
        const std::int32_t menu = tables_->menu.map(screen->menuType);
        if (menu == IdMap::kNone) {
            return false;
        }
        screen->menuType = menu;
        encode(packet, [&](io::ByteBuffer& out) { screen->write(out); });
        break;
    }
    case play::ContainerSetSlotPacket::kId: {
        auto slot = decode(packet, [](io::ByteBuffer& in) { return play::ContainerSetSlotPacket::read(in); });
        if (!slot.has_value()) {
            return true;
        }
        slot->item = stack(slot->item);
        encode(packet, [&](io::ByteBuffer& out) { slot->write(out); });
        break;
    }
    case play::ContainerSetContentPacket::kId: {
        auto content = decode(packet, [](io::ByteBuffer& in) { return play::ContainerSetContentPacket::read(in); });
        if (!content.has_value()) {
            return true;
        }
        for (std::optional<play::WireItemStack>& item : content->items) {
            item = stack(item);
        }
        content->carriedItem = stack(content->carriedItem);
        encode(packet, [&](io::ByteBuffer& out) { content->write(out); });
        break;
    }
    case play::SetEquipmentPacket::kId: {
        auto equipment = decode(packet, [](io::ByteBuffer& in) { return play::SetEquipmentPacket::read(in); });
        if (!equipment.has_value()) {
            return true;
        }
        for (play::SetEquipmentPacket::Slot& slot : equipment->slots) {
            slot.stack = stack(slot.stack);
        }
        encode(packet, [&](io::ByteBuffer& out) { equipment->write(out); });
        break;
    }
    case play::SetEntityDataPacket::kId:
        rewriteEntityData(packet);
        break;
    default:
        break;
    }
    return true;
}

bool ContentRewriter::rewriteSpawn(MutablePacket& packet) {
    auto spawn = decode(packet, [](io::ByteBuffer& in) { return play::SpawnEntityPacket::read(in); });
    if (!spawn.has_value()) {
        return true;
    }
    const std::int32_t type = tables_->entityType.map(spawn->entityType);
    if (type == IdMap::kNone) {
        // A player has no number of its own below 1.13, because it is never spawned as an entity
        // there: it keeps this build's and the link that makes it the packet those versions do have
        // recognises it by that.
        return spawn->entityType == tables_->nativePlayerEntityType();
    }
    entityTypes_[spawn->entityId] = spawn->entityType;
    if (tables_->entityType.substitutes(spawn->entityType)) {
        nameOver(packet, spawn->entityId, spawn->entityType);
    }
    spawn->entityType = type;
    encode(packet, [&](io::ByteBuffer& out) { spawn->write(out); });
    return true;
}

bool ContentRewriter::rewriteRecipes(MutablePacket& packet) const {
    const auto recipes = decode(packet, [](io::ByteBuffer& in) { return play::UpdateRecipesPacket::read(in); });
    if (!recipes.has_value()) {
        return true;
    }
    const std::int32_t kind = tables_->slotDisplay.map(play::SlotDisplay::kItemStackType);
    if (kind == IdMap::kNone) {
        return false;
    }
    std::vector<play::StonecutterOption> kept;
    kept.reserve(recipes->stonecutter.size());
    for (const play::StonecutterOption& option : recipes->stonecutter) {
        const std::int32_t result = tables_->item.map(option.resultItemId);
        if (result == IdMap::kNone) {
            continue;
        }
        std::vector<std::int32_t> inputs;
        inputs.reserve(option.inputItemIds.size());
        for (const std::int32_t input : option.inputItemIds) {
            const std::int32_t mapped = tables_->item.map(input);
            if (mapped != IdMap::kNone) {
                inputs.push_back(mapped);
            }
        }
        if (inputs.empty()) {
            continue;
        }
        kept.push_back({std::move(inputs), result, option.resultCount});
    }
    encode(packet, [&](io::ByteBuffer& out) {
        protocol::varInt::write(out, 0);
        protocol::varInt::write(out, static_cast<std::int32_t>(kept.size()));
        for (const play::StonecutterOption& option : kept) {
            protocol::varInt::write(out, static_cast<std::int32_t>(option.inputItemIds.size()) + 1);
            for (const std::int32_t id : option.inputItemIds) {
                protocol::varInt::write(out, id);
            }
            protocol::varInt::write(out, kind);
            protocol::varInt::write(out, option.resultItemId);
            protocol::varInt::write(out, option.resultCount);
            protocol::varInt::write(out, 0);
            protocol::varInt::write(out, 0);
        }
    });
    return true;
}

void ContentRewriter::rewriteEntityData(MutablePacket& packet) const {
    auto data = decode(packet, [](io::ByteBuffer& in) { return play::SetEntityDataPacket::read(in); });
    if (!data.has_value()) {
        return;
    }
    for (play::SetEntityDataPacket::Entry& entry : data->entries) {
        if (auto* item = std::get_if<play::SetEntityDataPacket::ItemValue>(&entry.value)) {
            item->stack = stack(item->stack);
        }
    }
    // What a mob standing in for another one is told about itself is held to what every entity has.
    // Past that the entries belong to the type this client is not being shown, and reading a size
    // where it expects a variant is how a client crashes.
    const auto found = entityTypes_.find(data->entityId);
    const std::int32_t nativeType = found == entityTypes_.end() ? kUnknownType : found->second;
    if (nativeType >= 0 && tables_->entityType.substitutes(nativeType)) {
        std::erase_if(data->entries, [](const play::SetEntityDataPacket::Entry& entry) {
            return entry.index > play::SetEntityDataPacket::kLastSharedIndex;
        });
    }
    if (auto translated = entityData_.rewrite(*data, nativeType)) {
        packet.body = *std::move(translated);
        return;
    }
    encode(packet, [&](io::ByteBuffer& out) { data->write(out); });
}

bool ContentRewriter::rewriteCommands(MutablePacket& packet) const {
    if (tables_->missingRegistries().empty()) {
        return true;
    }
    auto commands = decode(packet, [](io::ByteBuffer& in) { return play::CommandsPacket::read(in); });
    if (!commands.has_value()) {
        return true;
    }
    bool changed = false;
    for (play::CommandNodeData& node : commands->nodes) {
        if (!node.parser.has_value()) {
            continue;
        }
        const auto* registry = node.parser->as<play::ArgumentParser::RegistryProperties>();
        if (registry == nullptr || !tables_->missingRegistries().contains(registry->registry)) {
            continue;
        }
        changed = true;
        node.parser =
            play::ArgumentParser{play::argumentType::kBrigadierString,
                                 play::ArgumentParser::StringProperties{play::ArgumentParser::StringKind::SingleWord}};
    }
    if (!changed) {
        return true;
    }
    encode(packet, [&](io::ByteBuffer& out) { commands->write(out); });
    return true;
}

bool ContentRewriter::rewriteTags(MutablePacket& packet) const {
    auto tags = decode(packet, [](io::ByteBuffer& in) { return protocol::configuration::TagsPacket::read(in); });
    if (!tags.has_value()) {
        return true;
    }
    // A whole group goes when its registry is one that version does not have: the client looks the
    // registry up to hang the tags on it and gives up on the connection when it is not there, which
    // is what a real 1.21.10 client did with the timelines 26.1 invented.
    std::erase_if(tags->groups, [this](const protocol::configuration::TagGroup& group) {
        return tables_->missingRegistries().contains(group.registryId);
    });
    for (protocol::configuration::TagGroup& group : tags->groups) {
        const IdLookup* map = lookupFor(group.registryId);
        if (map == nullptr) {
            continue;
        }
        // A tag is a list of numbers, so a client that numbers its blocks differently is told that
        // the wrong blocks are logs. What has no counterpart drops out of the tag rather than
        // pointing at something else.
        for (protocol::configuration::TagEntry& tag : group.tags) {
            std::vector<std::int32_t> kept;
            kept.reserve(tag.ids.size());
            for (const std::int32_t id : tag.ids) {
                const std::int32_t mapped = map->map(id);
                if (mapped != IdMap::kNone) {
                    kept.push_back(mapped);
                }
            }
            tag.ids = std::move(kept);
        }
    }
    encode(packet, [&](io::ByteBuffer& out) { tags->write(out); });
    return true;
}

const IdLookup* ContentRewriter::lookupFor(const std::string& registryId) const {
    if (registryId == "minecraft:block") {
        return &tables_->block;
    }
    if (registryId == "minecraft:item") {
        return &tables_->item;
    }
    if (registryId == "minecraft:entity_type") {
        return &tables_->entityType;
    }
    if (registryId == "minecraft:game_event") {
        return &tables_->gameEvent;
    }
    return nullptr;
}

bool ContentRewriter::serverbound(ProtocolState state, MutablePacket& packet) {
    if (state != ProtocolState::Play || packet.nativeId() != play::ServerboundSetCreativeModeSlotPacket::kId) {
        return true;
    }
    auto creative =
        decode(packet, [](io::ByteBuffer& in) { return play::ServerboundSetCreativeModeSlotPacket::read(in); });
    if (!creative.has_value() || !creative->item.has_value() || creative->item->count <= 0) {
        return true;
    }
    const std::int32_t id = tables_->item.unmap(creative->item->itemId);
    if (id == IdMap::kNone) {
        return false;
    }
    creative->item->itemId = id;
    encode(packet, [&](io::ByteBuffer& out) { creative->write(out); });
    return true;
}

std::int32_t ContentRewriter::blockState(std::int32_t id) const {
    const std::int32_t mapped = tables_->blockStates.map(id);
    return mapped == IdMap::kNone ? 0 : mapped;
}

std::optional<play::WireItemStack> ContentRewriter::stack(const std::optional<play::WireItemStack>& source) const {
    if (!source.has_value() || source->count <= 0) {
        return source;
    }
    const std::int32_t id = tables_->item.map(source->itemId);
    if (id == IdMap::kNone) {
        return std::nullopt;
    }
    play::WireItemStack item = labelled(*source);
    item.itemId = id;
    // Every component, not only the ones carried as bytes: a release inserts component ids the same
    // way it inserts item ids, so `custom_name` is not the same number in two versions.
    std::vector<play::ItemComponent> components;
    components.reserve(item.components.size());
    for (const play::ItemComponent& component : item.components) {
        const std::int32_t moved = tables_->dataComponentType.map(component.id());
        if (moved == IdMap::kNone) {
            continue;
        }
        if (moved == component.id()) {
            components.push_back(component);
            continue;
        }
        io::ByteBuffer payload;
        component.writePayload(payload);
        components.emplace_back(moved, play::ItemComponent::Raw{payload.toVector()});
    }
    item.components = std::move(components);
    std::vector<std::int32_t> removed;
    removed.reserve(item.removedComponents.size());
    for (const std::int32_t component : item.removedComponents) {
        const std::int32_t moved = tables_->dataComponentType.map(component);
        if (moved != IdMap::kNone) {
            removed.push_back(moved);
        }
    }
    item.removedComponents = std::move(removed);
    return item;
}

play::WireItemStack ContentRewriter::labelled(play::WireItemStack item) const {
    if (labels_ == nullptr || !tables_->item.substitutes(item.itemId)) {
        return item;
    }
    const bool named = std::ranges::any_of(item.components, [](const play::ItemComponent& component) {
        return component.id() == play::dataComponent::kCustomName;
    });
    if (named) {
        return item;
    }
    std::optional<text::Component> label = labels_->itemLabel(item.itemId);
    if (!label.has_value()) {
        return item;
    }
    item.components.emplace_back(play::dataComponent::kCustomName, play::ItemComponent::Text{*std::move(label)});
    return item;
}

void ContentRewriter::nameOver(MutablePacket& packet, std::int32_t entityId, std::int32_t nativeType) const {
    if (labels_ == nullptr) {
        return;
    }
    std::optional<text::Component> label = labels_->entityLabel(nativeType);
    if (!label.has_value()) {
        return;
    }
    play::SetEntityDataPacket named;
    named.entityId = entityId;
    named.entries.push_back(
        {play::SetEntityDataPacket::kCustomNameIndex, play::SetEntityDataPacket::OptionalTextValue{*std::move(label)}});
    named.entries.push_back(
        {play::SetEntityDataPacket::kCustomNameVisibleIndex, play::SetEntityDataPacket::BoolValue{true}});
    // Through the same translation as any other entity data: this is the one packet nothing else has
    // written, so it is the one that would go out in this version's own numbers.
    std::vector<std::uint8_t> body;
    if (auto translated = entityData_.rewrite(named, nativeType)) {
        body = *std::move(translated);
    } else {
        io::ByteBuffer output;
        named.write(output);
        body = output.toVector();
    }
    packet.extras.emplace_back(play::SetEntityDataPacket::kId, std::move(body));
}

} // namespace basalt::wire::multiversion::java
