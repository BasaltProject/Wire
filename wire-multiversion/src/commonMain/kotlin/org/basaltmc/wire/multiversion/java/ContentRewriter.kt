package org.basaltmc.wire.multiversion.java

import org.basaltmc.wire.io.ByteBuffer
import org.basaltmc.wire.multiversion.ChunkRewriter
import org.basaltmc.wire.multiversion.ComposedTables
import org.basaltmc.wire.multiversion.IdLookup
import org.basaltmc.wire.multiversion.IdMap
import org.basaltmc.wire.multiversion.MutablePacket
import org.basaltmc.wire.protocol.ProtocolState
import org.basaltmc.wire.protocol.configuration.TagsPacket
import org.basaltmc.wire.protocol.play.ArgumentParser
import org.basaltmc.wire.protocol.play.ClientboundBlockUpdatePacket
import org.basaltmc.wire.protocol.play.CommandsPacket
import org.basaltmc.wire.protocol.play.ContainerSetContentPacket
import org.basaltmc.wire.protocol.play.ContainerSetSlotPacket
import org.basaltmc.wire.protocol.play.ExplodePacket
import org.basaltmc.wire.protocol.play.ItemComponent
import org.basaltmc.wire.protocol.play.LevelEventPacket
import org.basaltmc.wire.protocol.play.LevelParticlesPacket
import org.basaltmc.wire.protocol.play.MapChunkPacket
import org.basaltmc.wire.protocol.play.OpenScreenPacket
import org.basaltmc.wire.protocol.play.RemoveMobEffectPacket
import org.basaltmc.wire.protocol.play.SectionBlocksUpdatePacket
import org.basaltmc.wire.protocol.play.ServerboundSetCreativeModeSlotPacket
import org.basaltmc.wire.protocol.play.SetEntityDataPacket
import org.basaltmc.wire.protocol.play.SetEquipmentPacket
import org.basaltmc.wire.protocol.play.SpawnEntityPacket
import org.basaltmc.wire.protocol.play.StonecutterOption
import org.basaltmc.wire.protocol.play.UpdateAttributesPacket
import org.basaltmc.wire.protocol.play.UpdateMobEffectPacket
import org.basaltmc.wire.protocol.play.UpdateRecipesPacket
import org.basaltmc.wire.protocol.play.WireItemStack
import org.basaltmc.wire.protocol.writeVarInt

/**
 * Puts every id a packet carries into the numbering of the version at the far end of the chain.
 *
 * Once, on the packet as this server wrote it, rather than once per link. A link in the middle sees
 * a packet that has already been through the links above it, so it cannot read the fields: the
 * numbering is composed instead, and only the packets whose *shape* changed are touched by a link.
 *
 * What is covered is deliberately the list of what this server actually sends: chunks, block
 * updates, level events, particles, entity spawns, attributes, effects, menus, explosions, and the
 * item stacks in every packet that carries one.
 */
internal class ContentRewriter(
    private val tables: ComposedTables,
) : org.basaltmc.wire.multiversion.PacketContentRewriter {
    private val chunks =
        ChunkRewriter(
            tables.blockStates,
            tables.blockStateDirectBits,
            fluidCount = tables.fluidCountInSections,
        )

    /** False drops the packet: the far end has no id for what this one is about. */
    override fun clientbound(
        state: ProtocolState,
        packet: MutablePacket,
    ): Boolean {
        if (state == ProtocolState.CONFIGURATION && packet.nativeId == TagsPacket.ID) return rewriteTags(packet)
        if (state == ProtocolState.PLAY && packet.nativeId == CommandsPacket.ID) return rewriteCommands(packet)
        if (state != ProtocolState.PLAY) return true
        when (packet.nativeId) {
            MapChunkPacket.ID -> {
                val chunk = decode(packet) { MapChunkPacket.read(it) }
                encode(packet) { chunk.copy(chunkData = chunks.rewrite(chunk.chunkData)).write(it) }
            }
            ClientboundBlockUpdatePacket.ID -> {
                val update = decode(packet) { ClientboundBlockUpdatePacket.read(it) }
                encode(packet) { update.copy(blockStateId = blockState(update.blockStateId)).write(it) }
            }
            SectionBlocksUpdatePacket.ID -> {
                val update = decode(packet) { SectionBlocksUpdatePacket.read(it) }
                val states = IntArray(update.blockStateIds.size) { blockState(update.blockStateIds[it]) }
                encode(packet) { update.copy(blockStateIds = states).write(it) }
            }
            LevelEventPacket.ID -> {
                val event = decode(packet) { LevelEventPacket.read(it) }
                if (event.event != LevelEventPacket.PARTICLES_DESTROY_BLOCK) return true
                encode(packet) { event.copy(data = blockState(event.data)).write(it) }
            }
            LevelParticlesPacket.ID -> {
                val particles = decode(packet) { LevelParticlesPacket.read(it) }
                val type = tables.particleType.map(particles.particleTypeId)
                if (type == IdMap.NONE) return false
                encode(packet) { particles.copy(particleTypeId = type).write(it) }
            }
            ExplodePacket.ID -> {
                val explode = decode(packet) { ExplodePacket.read(it) }
                val particle = tables.particleType.map(explode.particleId)
                if (particle == IdMap.NONE) return false
                val thrown =
                    explode.blockParticles.mapNotNull { one ->
                        val id = tables.particleType.map(one.particleId)
                        if (id == IdMap.NONE) null else one.copy(particleId = id)
                    }
                encode(packet) { explode.copy(particleId = particle, blockParticles = thrown).write(it) }
            }
            SpawnEntityPacket.ID -> {
                val spawn = decode(packet) { SpawnEntityPacket.read(it) }
                val type = tables.entityType.map(spawn.entityType)
                if (type == IdMap.NONE) {
                    // A player has no number of its own below 1.13, because it is never spawned as
                    // an entity there: it keeps this build's and the link that makes it the packet
                    // those versions do have recognises it by that.
                    if (spawn.entityType != tables.nativePlayerEntityType) return false
                    return true
                }
                encode(packet) { spawn.copy(entityType = type).write(it) }
            }
            UpdateRecipesPacket.ID -> {
                val recipes = decode(packet) { UpdateRecipesPacket.read(it) }
                val kind = tables.slotDisplay.map(UpdateRecipesPacket.ITEM_STACK_SLOT_DISPLAY)
                if (kind == IdMap.NONE) return false
                val kept =
                    recipes.stonecutter.mapNotNull { option ->
                        val result = tables.item.map(option.resultItemId)
                        if (result == IdMap.NONE) return@mapNotNull null
                        val inputs = option.inputItemIds.map { tables.item.map(it) }.filter { it != IdMap.NONE }
                        if (inputs.isEmpty()) return@mapNotNull null
                        StonecutterOption(inputs, result, option.resultCount)
                    }
                encode(packet) { output ->
                    output.writeVarInt(0)
                    output.writeVarInt(kept.size)
                    for (option in kept) {
                        output.writeVarInt(option.inputItemIds.size + 1)
                        for (id in option.inputItemIds) output.writeVarInt(id)
                        output.writeVarInt(kind)
                        output.writeVarInt(option.resultItemId)
                        output.writeVarInt(option.resultCount)
                        output.writeVarInt(0)
                        output.writeVarInt(0)
                    }
                }
            }
            UpdateAttributesPacket.ID -> {
                val attributes = decode(packet) { UpdateAttributesPacket.read(it) }
                val kept =
                    attributes.attributes.mapNotNull { snapshot ->
                        val id = tables.attribute.map(snapshot.attribute)
                        if (id == IdMap.NONE) null else snapshot.copy(attribute = id)
                    }
                encode(packet) { attributes.copy(attributes = kept).write(it) }
            }
            UpdateMobEffectPacket.ID -> {
                val effect = decode(packet) { UpdateMobEffectPacket.read(it) }
                val id = tables.mobEffect.map(effect.effect)
                if (id == IdMap.NONE) return false
                encode(packet) { effect.copy(effect = id).write(it) }
            }
            RemoveMobEffectPacket.ID -> {
                val effect = decode(packet) { RemoveMobEffectPacket.read(it) }
                val id = tables.mobEffect.map(effect.effect)
                if (id == IdMap.NONE) return false
                encode(packet) { effect.copy(effect = id).write(it) }
            }
            OpenScreenPacket.ID -> {
                val screen = decode(packet) { OpenScreenPacket.read(it) }
                val menu = tables.menu.map(screen.menuType)
                if (menu == IdMap.NONE) return false
                encode(packet) { screen.copy(menuType = menu).write(it) }
            }
            ContainerSetSlotPacket.ID -> {
                val slot = decode(packet) { ContainerSetSlotPacket.read(it) }
                encode(packet) { slot.copy(item = stack(slot.item)).write(it) }
            }
            ContainerSetContentPacket.ID -> {
                val content = decode(packet) { ContainerSetContentPacket.read(it) }
                encode(packet) {
                    content
                        .copy(
                            items =
                                content.items.map { item ->
                                    stack(item)
                                },
                            carriedItem = stack(content.carriedItem),
                        ).write(it)
                }
            }
            SetEquipmentPacket.ID -> {
                val equipment = decode(packet) { SetEquipmentPacket.read(it) }
                val slots = equipment.slots.map { it.copy(stack = stack(it.stack)) }
                encode(packet) { equipment.copy(slots = slots).write(it) }
            }
            SetEntityDataPacket.ID -> {
                val data = decode(packet) { SetEntityDataPacket.read(it) }
                val entries =
                    data.entries.map { entry ->
                        if (entry is SetEntityDataPacket.Entry.Item) entry.copy(stack = stack(entry.stack)) else entry
                    }
                encode(packet) { data.copy(entries = entries).write(it) }
            }
        }
        return true
    }

    /**
     * The command tree, whose arguments may name a registry the far end does not have.
     *
     * The client looks that registry up while it reads the tree and gives up on the whole packet
     * when it is not there, so an argument like that becomes a plain word: the command can still be
     * typed and the server still parses it, which is where the name is checked anyway.
     */
    private fun rewriteCommands(packet: MutablePacket): Boolean {
        if (tables.missingRegistries.isEmpty()) return true
        val commands = decode(packet) { CommandsPacket.read(it) }
        var changed = false
        val nodes =
            commands.nodes.map { node ->
                val registry = registryOf(node.parser) ?: return@map node
                if (registry !in tables.missingRegistries) return@map node
                changed = true
                node.copy(parser = ArgumentParser.BrigadierString(ArgumentParser.BrigadierString.Kind.SINGLE_WORD))
            }
        if (!changed) return true
        encode(packet) { commands.copy(nodes = nodes).write(it) }
        return true
    }

    private fun registryOf(parser: ArgumentParser?): String? =
        when (parser) {
            is ArgumentParser.Resource -> parser.registry
            is ArgumentParser.ResourceKey -> parser.registry
            is ArgumentParser.ResourceOrTag -> parser.registry
            is ArgumentParser.ResourceOrTagKey -> parser.registry
            is ArgumentParser.ResourceSelector -> parser.registry
            else -> null
        }

    /**
     * The tags, whose contents are ids of whatever registry they are about.
     *
     * A tag is a list of numbers, so a client that numbers its blocks differently is told that the
     * wrong blocks are logs. What has no counterpart drops out of the tag rather than pointing at
     * something else.
     *
     * A whole group goes when its registry is one that version does not have: the client looks the
     * registry up to hang the tags on it and gives up on the connection when it is not there, which
     * is what a real 1.21.10 client did with the timelines 26.1 invented.
     */
    private fun rewriteTags(packet: MutablePacket): Boolean {
        val tags = decode(packet) { TagsPacket.read(it) }
        val groups =
            tags.groups.filter { it.registryId !in tables.missingRegistries }.map { group ->
                val map = lookupFor(group.registryId) ?: return@map group
                group.copy(
                    tags =
                        group.tags.map { tag ->
                            tag.copy(ids = tag.ids.map { map.map(it) }.filter { it != IdMap.NONE })
                        },
                )
            }
        encode(packet) { TagsPacket(groups).write(it) }
        return true
    }

    private fun lookupFor(registryId: String): IdLookup? =
        when (registryId) {
            "minecraft:block" -> tables.block
            "minecraft:item" -> tables.item
            "minecraft:entity_type" -> tables.entityType
            "minecraft:game_event" -> tables.gameEvent
            else -> null
        }

    /** The way back, for the one packet a client sends that carries an item. */
    override fun serverbound(
        state: ProtocolState,
        packet: MutablePacket,
    ): Boolean {
        if (state != ProtocolState.PLAY || packet.nativeId != ServerboundSetCreativeModeSlotPacket.ID) return true
        val creative = decode(packet) { ServerboundSetCreativeModeSlotPacket.read(it) }
        val item = creative.item
        if (item == null || item.count <= 0) return true
        val id = tables.item.unmap(item.itemId)
        if (id == IdMap.NONE) return false
        encode(packet) { creative.copy(item = item.copy(itemId = id)).write(it) }
        return true
    }

    private fun blockState(id: Int): Int {
        val mapped = tables.blockStates.map(id)
        return if (mapped == IdMap.NONE) 0 else mapped
    }

    private fun stack(item: WireItemStack?): WireItemStack? {
        if (item == null || item.count <= 0) return item
        val id = tables.item.map(item.itemId)
        if (id == IdMap.NONE) return null
        // Every component, not only the ones carried as bytes: a release inserts component ids the
        // same way it inserts item ids, so `custom_name` is not the same number in two versions.
        val components =
            item.components.mapNotNull { component ->
                val moved = tables.dataComponentType.map(component.id)
                when {
                    moved == IdMap.NONE -> null
                    moved == component.id -> component
                    else -> ItemComponent.Raw(moved, payloadOf(component))
                }
            }
        val removed =
            item.removedComponents.mapNotNull {
                val moved = tables.dataComponentType.map(it)
                if (moved == IdMap.NONE) null else moved
            }
        return item.copy(itemId = id, components = components, removedComponents = removed)
    }

    private fun payloadOf(component: ItemComponent): ByteArray {
        val buffer = ByteBuffer()
        component.writePayload(buffer)
        return buffer.toByteArray()
    }

    private inline fun <T> decode(
        packet: MutablePacket,
        read: (ByteBuffer) -> T,
    ): T = read(ByteBuffer().apply { writeBytes(packet.body) })

    private inline fun encode(
        packet: MutablePacket,
        write: (ByteBuffer) -> Unit,
    ) {
        val output = ByteBuffer()
        write(output)
        packet.body = output.toByteArray()
    }
}
