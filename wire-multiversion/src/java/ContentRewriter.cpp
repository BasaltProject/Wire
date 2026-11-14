#include <basalt/wire/multiversion/ProtocolVersions.hpp>
#include <basalt/wire/multiversion/java/AdvancementLayouts.hpp>
#include <basalt/wire/multiversion/java/BlockEntityStates.hpp>
#include <basalt/wire/multiversion/java/BlockEntityTypes.hpp>
#include <basalt/wire/multiversion/java/ClickItems.hpp>
#include <basalt/wire/multiversion/java/ContentRewriter.hpp>
#include <basalt/wire/multiversion/java/LegacyItemTags.hpp>
#include <basalt/wire/multiversion/java/LegacyStacks.hpp>
#include <basalt/wire/multiversion/java/MapLayouts.hpp>
#include <basalt/wire/multiversion/java/NativeParticles.hpp>
#include <basalt/wire/multiversion/java/OlderBlockEntities.hpp>
#include <basalt/wire/multiversion/java/OlderCommands.hpp>
#include <basalt/wire/multiversion/java/OlderComponents.hpp>
#include <basalt/wire/multiversion/java/OlderParticles.hpp>
#include <basalt/wire/multiversion/java/RecipeDisplays.hpp>
#include <basalt/wire/multiversion/java/Stack769.hpp>
#include <basalt/wire/multiversion/java/StackComponents1_21_4.hpp>
#include <basalt/wire/protocol/ProtocolBuffer.hpp>
#include <basalt/wire/protocol/VarInt.hpp>
#include <basalt/wire/protocol/configuration/ConfigurationPackets.hpp>
#include <basalt/wire/protocol/play/BlockEntityPackets.hpp>
#include <basalt/wire/protocol/play/CommandPackets.hpp>
#include <basalt/wire/protocol/play/EntityData.hpp>
#include <basalt/wire/protocol/play/EntityPackets.hpp>
#include <basalt/wire/protocol/play/InventoryPackets.hpp>
#include <basalt/wire/protocol/play/ItemComponent.hpp>
#include <basalt/wire/protocol/play/MapPackets.hpp>
#include <basalt/wire/protocol/play/PlayPackets.hpp>
#include <basalt/wire/protocol/play/PlayerStatePackets.hpp>
#include <basalt/wire/protocol/play/RecipePackets.hpp>
#include <basalt/wire/protocol/play/SimplePackets.hpp>
#include <basalt/wire/protocol/play/WorldPackets.hpp>

#include <algorithm>
#include <cstdint>
#include <utility>

namespace basalt::wire::multiversion::java {
namespace {

// parity: ClientboundGameEventPacket, the newest event each of these versions has.
constexpr std::uint8_t kGuardianCurse = 10;
constexpr std::uint8_t kImmediateRespawn = 11;
constexpr std::uint8_t kLimitedCrafting = 12;
constexpr std::uint8_t kLevelChunksLoadStart = 13;

constexpr std::int32_t kSetCursorItem = 98;
constexpr std::int32_t kSetPlayerInventory = 110;

using protocol::ProtocolState;
namespace play = protocol::play;

template <typename Read>
auto decode(const MutablePacket& packet, Read&& read) {
    io::ByteBuffer input = io::ByteBuffer::wrap(packet.body);
    return std::forward<Read>(read)(input);
}

template <typename Write>
void encode(MutablePacket& packet, Write&& write) {
    io::ByteBuffer output;
    std::forward<Write>(write)(output);
    packet.body = output.toVector();
}

constexpr std::int32_t kStone = 1;

} // namespace

ContentRewriter::ContentRewriter(const ComposedTables& tables, const SubstitutionLabels* labels, std::int32_t farEnd,
                                 std::vector<std::int32_t> links, bool backendCountsFluids)
    : tables_(&tables), labels_(labels), farEnd_(farEnd),
      entityData_(
          farEnd, std::move(links), [this](const std::optional<play::WireItemStack>& item) { return stack(item); },
          [this](std::int32_t state) { return blockState(state); }),
      spawns_(tables, labels, farEnd, entityData_), paintings_(farEnd), countsFluids_(backendCountsFluids),
      chunks_(tables.blockStates, ChunkRewriter::Options{.olderDirectBits = tables.blockStateDirectBits(),
                                                         .fluidCount = tables.fluidCountInSections(),
                                                         .readsFluidCount = backendCountsFluids}) {}

bool ContentRewriter::clientbound(ProtocolState state, MutablePacket& packet) {
    if (state != ProtocolState::Play || !paintings_.holding()) {
        return rewriteClientbound(state, packet);
    }
    if (const std::optional<std::int32_t> motive = paintings_.motiveIn(packet)) {
        rewriteEntityData(packet);
        std::vector<std::uint8_t> data = std::move(packet.body);
        paintings_.spawn(packet, *motive);
        io::ByteBuffer entries = io::ByteBuffer::wrap(data);
        static_cast<void>(protocol::varInt::read(entries));
        if (entries.readable().size() > 1) {
            packet.extras.emplace_back(play::SetEntityDataPacket::kId, std::move(data));
        }
        return true;
    }
    std::optional<MutablePacket::Continued> held = paintings_.release(packet);
    const bool kept = rewriteClientbound(state, packet);
    if (!held.has_value()) {
        return kept;
    }
    if (!kept) {
        packet.reset(held->id, held->nativeId, std::move(held->body));
        packet.spawn = held->spawn;
        return true;
    }
    packet.continued.push_back(*std::move(held));
    return true;
}

bool ContentRewriter::rewriteClientbound(ProtocolState state, MutablePacket& packet) {
    if (state == ProtocolState::Configuration && packet.nativeId() == protocol::configuration::TagsPacket::kId) {
        return rewriteTags(packet);
    }
    if (state == ProtocolState::Configuration &&
        packet.nativeId() == protocol::configuration::RegistryDataPacket::kId && farEnd_ < protocolVersions::kV1_20_5) {
        paintings_.learn(packet.body);
        if (auto names = legacyItemTags::enchantmentNames(packet.body)) {
            enchantments_ = *std::move(names);
        }
        return true;
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
        const BlockEntityStates::Chunk states{chunk->chunkData, countsFluids_};
        std::erase_if(chunk->blockEntities, [&](play::ChunkBlockEntityInfo& blockEntity) {
            const std::optional<std::int32_t> state = olderBlockEntities::readsState(blockEntity.typeId, farEnd_)
                                                          ? states.at(blockEntity.packedXZ, blockEntity.y)
                                                          : std::nullopt;
            if (farEnd_ < protocolVersions::kV1_20_5) {
                olderBlockEntities::rewrite(blockEntity.typeId, blockEntity.tag, farEnd_, state);
            }
            if (!renumbersBlockEntities()) {
                return false;
            }
            blockEntity.typeId = blockEntityTypes::idAt(farEnd_, blockEntity.typeId);
            return blockEntity.typeId == blockEntityTypes::kNone;
        });
        chunk->chunkData = chunks_.rewrite(chunk->chunkData);
        encode(packet, [&](io::ByteBuffer& out) { chunk->write(out); });
        break;
    }
    case play::BlockEntityDataPacket::kId: {
        if (!renumbersBlockEntities() && farEnd_ >= protocolVersions::kV1_20_5) {
            break;
        }
        auto data = decode(packet, [](io::ByteBuffer& in) { return play::BlockEntityDataPacket::read(in); });
        if (!data.has_value()) {
            return true;
        }
        if (farEnd_ < protocolVersions::kV1_20_5) {
            olderBlockEntities::rewrite(data->typeId, data->tag, farEnd_, placed_.at(data->pos.pack()));
        }
        if (renumbersBlockEntities()) {
            data->typeId = blockEntityTypes::idAt(farEnd_, data->typeId);
        }
        if (data->typeId == blockEntityTypes::kNone) {
            return false;
        }
        encode(packet, [&](io::ByteBuffer& out) { data->write(out); });
        break;
    }
    case play::ClientboundBlockUpdatePacket::kId: {
        auto update = decode(packet, [](io::ByteBuffer& in) { return play::ClientboundBlockUpdatePacket::read(in); });
        if (!update.has_value()) {
            return true;
        }
        if (farEnd_ < protocolVersions::kV1_13) {
            placed_.remember(update->pos.pack(), update->blockStateId);
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
            if (farEnd_ < protocolVersions::kV1_13) {
                placed_.remember(update->sectionX, update->sectionY, update->sectionZ, change.position,
                                 change.blockStateId);
            }
            change.blockStateId = blockState(change.blockStateId);
        }
        encode(packet, [&](io::ByteBuffer& out) { update->write(out); });
        break;
    }
    case play::BlockEventPacket::kId: {
        auto event = decode(packet, [](io::ByteBuffer& in) { return play::BlockEventPacket::read(in); });
        if (!event.has_value()) {
            return true;
        }
        const std::int32_t mapped = tables_->block.map(event->blockId);
        if (mapped == IdMap::kNone) {
            return false;
        }
        event->blockId = mapped;
        encode(packet, [&](io::ByteBuffer& out) { event->write(out); });
        break;
    }
    case play::GameEventPacket::kId:
        // A client drops the whole connection on an event it has no number for.
        return packet.body.empty() || packet.body.front() <= newestGameEvent();
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
        io::ByteBuffer input = io::ByteBuffer::wrap(packet.body);
        const std::int32_t nativeType = protocol::varInt::read(input);
        if (input.failed()) {
            return true;
        }
        const std::int32_t mapped = tables_->particleType.map(nativeType);
        if (mapped == IdMap::kNone) {
            return false;
        }
        if (farEnd_ < protocolVersions::kV26_1) {
            return olderParticle(packet, nativeType, mapped);
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
    case play::LoginPlayPacket::kId: {
        io::ByteBuffer input = io::ByteBuffer::wrap(packet.body);
        const std::int32_t entityId = input.readInt();
        if (!input.failed()) {
            spawns_.player(entityId);
        }
        break;
    }
    case play::SpawnEntityPacket::kId:
        paintings_.hold(packet);
        return spawns_.spawn(packet);
    case play::RemoveEntitiesPacket::kId: {
        const auto gone = decode(packet, [](io::ByteBuffer& in) { return play::RemoveEntitiesPacket::read(in); });
        if (gone.has_value()) {
            spawns_.forget(gone->entityIds);
        }
        break;
    }
    case play::RecipeBookAddPacket::kId:
        if (farEnd_ != protocolVersions::kNative) {
            recipeDisplays::rewriteBookAdd(packet, *tables_);
        }
        break;
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
    case kSetCursorItem: {
        io::ByteBuffer in = io::ByteBuffer::wrap(packet.body);
        auto item = play::WireItemStack::read(in);
        if (in.failed()) {
            return true;
        }
        io::ByteBuffer out;
        play::WireItemStack::writeOptional(out, stack(item));
        packet.body = out.toVector();
        break;
    }
    case kSetPlayerInventory: {
        io::ByteBuffer in = io::ByteBuffer::wrap(packet.body);
        const std::int32_t slot = protocol::varInt::read(in);
        auto item = play::WireItemStack::read(in);
        if (in.failed()) {
            return true;
        }
        io::ByteBuffer out;
        protocol::varInt::write(out, slot);
        play::WireItemStack::writeOptional(out, stack(item));
        packet.body = out.toVector();
        break;
    }
    case play::MerchantOffersPacket::kId: {
        auto offers = decode(packet, [](io::ByteBuffer& in) { return play::MerchantOffersPacket::read(in); });
        if (!offers.has_value()) {
            return true;
        }
        for (play::MerchantOffer& offer : offers->offers) {
            offer.costA.itemId = tables_->item.map(offer.costA.itemId);
            if (offer.costB.has_value()) {
                offer.costB->itemId = tables_->item.map(offer.costB->itemId);
            }
            if (const auto moved = stack(offer.result); moved.has_value()) {
                offer.result = *moved;
            }
        }
        encode(packet, [&](io::ByteBuffer& out) { offers->write(out); });
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
    case play::UpdateAdvancementsPacket::kId:
        rewriteAdvancements(packet);
        break;
    case play::MapItemDataPacket::kId: {
        const auto map = decode(packet, [](io::ByteBuffer& in) { return play::MapItemDataPacket::read(in); });
        if (map.has_value()) {
            encode(packet, [&](io::ByteBuffer& out) { mapLayouts::write(out, *map, farEnd_); });
        }
        break;
    }
    default:
        break;
    }
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
    std::vector<play::ItemPropertySet> sets;
    sets.reserve(recipes->propertySets.size());
    for (const play::ItemPropertySet& set : recipes->propertySets) {
        play::ItemPropertySet mappedSet{.key = set.key, .itemIds = {}};
        for (const std::int32_t item : set.itemIds) {
            if (const std::int32_t mapped = tables_->item.map(item); mapped != IdMap::kNone) {
                mappedSet.itemIds.push_back(mapped);
            }
        }
        sets.push_back(std::move(mappedSet));
    }
    encode(packet, [&](io::ByteBuffer& out) {
        protocol::varInt::write(out, static_cast<std::int32_t>(sets.size()));
        for (const play::ItemPropertySet& set : sets) {
            protocol::writeString(out, set.key);
            protocol::varInt::write(out, static_cast<std::int32_t>(set.itemIds.size()));
            for (const std::int32_t id : set.itemIds) {
                protocol::varInt::write(out, id);
            }
        }
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
    auto data = decode(packet, [](io::ByteBuffer& in) { return EntityDataRewriter::read(in); });
    if (!data.has_value()) {
        return;
    }
    for (play::SetEntityDataPacket::Entry& entry : data->entries) {
        if (auto* item = std::get_if<play::SetEntityDataPacket::ItemValue>(&entry.value)) {
            item->stack = stack(item->stack);
        }
    }
    const std::int32_t nativeType = spawns_.typeOf(data->entityId);
    if (auto translated = entityData_.rewrite(*data, nativeType, spawns_.lastKept(nativeType))) {
        packet.body = *std::move(translated);
        return;
    }
    encode(packet, [&](io::ByteBuffer& out) { data->write(out); });
}

bool ContentRewriter::rewriteCommands(MutablePacket& packet) const {
    const bool older = farEnd_ != protocolVersions::kNative;
    if (tables_->missingRegistries().empty() && !older) {
        return true;
    }
    auto commands = decode(packet, [](io::ByteBuffer& in) { return play::CommandsPacket::read(in); });
    if (!commands.has_value()) {
        return true;
    }
    const auto missing = [this](const std::string& registry) {
        return tables_->missingRegistries().contains(registry);
    };
    if (older) {
        packet.body = olderCommands::write(*commands, farEnd_, missing);
        packet.becomes(olderCommands::kTranslatedCommands);
        return true;
    }
    bool changed = false;
    for (play::CommandNodeData& node : commands->nodes) {
        const auto* registry =
            node.parser.has_value() ? node.parser->as<play::ArgumentParser::RegistryProperties>() : nullptr;
        if (registry != nullptr && missing(registry->registry)) {
            changed = true;
            node.parser = play::ArgumentParser{
                play::argumentType::kBrigadierString,
                play::ArgumentParser::StringProperties{play::ArgumentParser::StringKind::SingleWord}};
        }
    }
    if (changed) {
        encode(packet, [&](io::ByteBuffer& out) { commands->write(out); });
    }
    return true;
}

bool ContentRewriter::rewriteTags(MutablePacket& packet) const {
    auto tags = decode(packet, [](io::ByteBuffer& in) { return protocol::configuration::TagsPacket::read(in); });
    if (!tags.has_value()) {
        return true;
    }
    std::erase_if(tags->groups, [this](const protocol::configuration::TagGroup& group) {
        return tables_->missingRegistries().contains(group.registryId);
    });
    for (protocol::configuration::TagGroup& group : tags->groups) {
        const IdLookup* map = lookupFor(group.registryId);
        if (map == nullptr) {
            continue;
        }
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
    if (state == ProtocolState::Play && packet.nativeId() == play::ServerboundContainerClickPacket::kId) {
        return renumberClickItems(packet.body, tables_->item);
    }
    if (state != ProtocolState::Play || packet.nativeId() != play::ServerboundSetCreativeModeSlotPacket::kId) {
        return true;
    }
    auto creative =
        decode(packet, [this](io::ByteBuffer& in) -> std::optional<play::ServerboundSetCreativeModeSlotPacket> {
            if (farEnd_ < protocolVersions::kV1_20_5 || farEnd_ >= protocolVersions::kV1_21_5) {
                return play::ServerboundSetCreativeModeSlotPacket::read(in);
            }
            // Before 1.21.5 a client sent its components without lengths, each in that version's own shape.
            const std::int16_t slot = in.readShort();
            io::ByteBuffer native;
            const stack769::SourceScope source{stack769::sourceFor(farEnd_)};
            if (!stack769::convert(in, native) || in.readableBytes() != 0) {
                return std::nullopt;
            }
            return play::ServerboundSetCreativeModeSlotPacket{slot, play::WireItemStack::read(native)};
        });
    if (!creative.has_value()) {
        return farEnd_ < protocolVersions::kV1_20_5 || farEnd_ >= protocolVersions::kV1_21_5;
    }
    if (!creative->item.has_value() || creative->item->count <= 0) {
        encode(packet, [&](io::ByteBuffer& out) { creative->write(out); });
        return true;
    }
    std::int32_t id = tables_->item.unmap(creative->item->itemId);
    std::int32_t data = 0;
    if (id == IdMap::kNone && farEnd_ < protocolVersions::kV1_13) {
        // A data value no variant claims is the damage of the item it sits beside.
        data = static_cast<std::int32_t>(static_cast<std::uint32_t>(creative->item->itemId) >> 16);
        id = tables_->item.unmap(creative->item->itemId & 0xFFFF);
    }
    if (id == IdMap::kNone) {
        return false;
    }
    creative->item->itemId = id;
    if (farEnd_ < protocolVersions::kV1_20_5) {
        legacyItemTags::fromStack(*creative->item, data, farEnd_, enchantments_);
    }
    encode(packet, [&](io::ByteBuffer& out) { creative->write(out); });
    return true;
}

void ContentRewriter::rewriteAdvancements(MutablePacket& packet) const {
    auto advancements = decode(packet, [](io::ByteBuffer& in) { return play::UpdateAdvancementsPacket::read(in); });
    if (!advancements.has_value()) {
        return;
    }
    for (play::AdvancementEntry& entry : advancements->added) {
        if (!entry.display.has_value()) {
            continue;
        }
        // An icon is never empty: one the far end has no item for is drawn as stone.
        entry.display->icon = stack(entry.display->icon).value_or(play::WireItemStack{.itemId = kStone, .count = 1});
    }
    encode(packet, [&](io::ByteBuffer& out) { advancementLayouts::write(out, *advancements, farEnd_); });
}

std::uint8_t ContentRewriter::newestGameEvent() const noexcept {
    if (farEnd_ >= protocolVersions::kV1_20_3) {
        return kLevelChunksLoadStart;
    }
    if (farEnd_ >= protocolVersions::kV1_20_2) {
        return kLimitedCrafting;
    }
    return farEnd_ >= protocolVersions::kV1_15 ? kImmediateRespawn : kGuardianCurse;
}

std::int32_t ContentRewriter::blockState(std::int32_t id) const {
    return tables_->blockStates.map(id) == IdMap::kNone ? 0 : tables_->blockStates.map(id);
}

std::optional<play::WireItemStack> ContentRewriter::stack(const std::optional<play::WireItemStack>& source) const {
    if (!source.has_value() || source->count <= 0) {
        return source;
    }
    std::int32_t id = tables_->item.map(source->itemId);
    if (id == IdMap::kNone && farEnd_ < protocolVersions::kV1_9 && legacyItemTags::isPotion(source->itemId)) {
        // 1.8 throws a potion by its data value, not by a separate item.
        id = tables_->item.map(legacyItemTags::kPotion);
    }
    if (id == IdMap::kNone) {
        return std::nullopt;
    }
    play::WireItemStack item = labelled(*source);
    item.itemId = id;
    if (farEnd_ < protocolVersions::kV1_20_5) {
        return legacyItemTags::toStack(std::move(item), source->itemId, farEnd_, enchantments_);
    }
    const bool hideTooltip =
        farEnd_ < protocolVersions::kV1_21_5 &&
        stackComponents1_21_4::rewrite(item, farEnd_, [this](const auto& held) { return stack(held); });
    if (farEnd_ >= protocolVersions::kV1_21_5 && farEnd_ < protocolVersions::kV26_1) {
        stackComponents1_21_4::nestedStacks(item, farEnd_, [this](const auto& held) { return stack(held); });
    }
    std::vector<play::ItemComponent> components;
    components.reserve(item.components.size());
    for (const play::ItemComponent& component : item.components) {
        const std::int32_t moved = tables_->dataComponentType.map(component.id());
        if (moved == IdMap::kNone) {
            continue;
        }
        const auto converted = olderComponents::template26_2(component, *tables_, farEnd_);
        if (!converted.has_value()) {
            continue;
        }
        if (moved == component.id()) {
            components.push_back(*converted);
            continue;
        }
        io::ByteBuffer payload;
        converted->writePayload(payload);
        components.emplace_back(moved, play::ItemComponent::Raw{payload.toVector()});
    }
    if (hideTooltip) {
        components.emplace_back(stackComponents1_21_4::hideTooltipId(farEnd_), play::ItemComponent::Raw{});
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

bool ContentRewriter::olderParticle(MutablePacket& packet, std::int32_t nativeType, std::int32_t mapped) const {
    auto particle = decode(packet, [nativeType](io::ByteBuffer& in) {
        return play::LevelParticlesPacket::read(in, nativeParticles::shapeOf(nativeType));
    });
    if (farEnd_ < protocolVersions::kV1_13 || !particle.has_value()) {
        return false;
    }
    auto body = olderParticles::write(
        *particle, mapped, farEnd_, [this](std::int32_t state) { return blockState(state); },
        [this](io::ByteBuffer& out, const play::ParticleOptions::Item& item) {
            legacyStacks::write(
                out,
                stack(play::WireItemStack{.itemId = item.itemId, .count = item.count, .components = item.components}),
                farEnd_);
        });
    if (!body.has_value()) {
        return false;
    }
    packet.body = *std::move(body);
    packet.becomes(olderParticles::kTranslatedParticles);
    return true;
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

} // namespace basalt::wire::multiversion::java
