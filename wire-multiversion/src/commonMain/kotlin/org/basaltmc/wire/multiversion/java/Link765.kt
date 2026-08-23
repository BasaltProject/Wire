package org.basaltmc.wire.multiversion.java

import org.basaltmc.wire.io.ByteBuffer
import org.basaltmc.wire.multiversion.ComposedTables
import org.basaltmc.wire.multiversion.MutablePacket
import org.basaltmc.wire.multiversion.VersionTables
import org.basaltmc.wire.nbt.NbtCompound
import org.basaltmc.wire.protocol.ProtocolState
import org.basaltmc.wire.protocol.play.ContainerSetContentPacket
import org.basaltmc.wire.protocol.play.ContainerSetSlotPacket
import org.basaltmc.wire.protocol.play.ItemComponent
import org.basaltmc.wire.protocol.play.ServerboundSetCreativeModeSlotPacket
import org.basaltmc.wire.protocol.play.SetEquipmentPacket
import org.basaltmc.wire.protocol.play.UpdateAttributesPacket
import org.basaltmc.wire.protocol.play.WireItemStack
import org.basaltmc.wire.protocol.readAnonymousNbt
import org.basaltmc.wire.protocol.readUuid
import org.basaltmc.wire.protocol.readVarInt
import org.basaltmc.wire.protocol.writeAnonymousNbt
import org.basaltmc.wire.protocol.writeBoolean
import org.basaltmc.wire.protocol.writeProtocolString
import org.basaltmc.wire.protocol.writeUuid
import org.basaltmc.wire.protocol.writeVarInt
import org.basaltmc.wire.text.ComponentJson

/**
 * 1.20.5 to 1.20.4, where an item stops being a set of components and goes back to being NBT.
 *
 * 1.20.5 replaced the tag an item carried with a list of components. Below it a stack is
 * `present, id, count, nbt`, so every stack this server writes has to be turned back into that.
 *
 * What survives: the item, how many, its damage and its name. The rest of what a component can say
 * is dropped rather than guessed at. The attributes go back to being named here too.
 */
@OptIn(kotlin.uuid.ExperimentalUuidApi::class)
internal class Link765(
    tables: VersionTables,
    private val target: ComposedTables,
) : RenumberingLink(tables) {
    override fun clientbound(
        state: ProtocolState,
        packet: MutablePacket,
    ): Boolean {
        if (state != ProtocolState.PLAY) return true
        when (packet.nativeId) {
            UpdateAttributesPacket.ID -> nameAttributes(packet)
            ContainerSetSlotPacket.ID -> {
                // The link to 1.21 already put the container in one byte, so this reads that.
                val input = decode(packet) { it }
                val container = input.readByte()
                val stateId = input.readVarInt()
                val slot = input.readShort()
                val item = WireItemStack.read(input)
                encode(packet) { output ->
                    output.writeByte(container)
                    output.writeVarInt(stateId)
                    output.writeShort(slot)
                    writeOldStack(output, item)
                }
            }
            ContainerSetContentPacket.ID -> {
                val input = decode(packet) { it }
                val container = input.readByte()
                val stateId = input.readVarInt()
                val items = List(input.readVarInt()) { WireItemStack.read(input) }
                val carried = WireItemStack.read(input)
                encode(packet) { output ->
                    output.writeByte(container)
                    output.writeVarInt(stateId)
                    output.writeVarInt(items.size)
                    for (item in items) writeOldStack(output, item)
                    writeOldStack(output, carried)
                }
            }
            SetEquipmentPacket.ID -> {
                val equipment = decode(packet) { SetEquipmentPacket.read(it) }
                encode(packet) { output ->
                    output.writeVarInt(equipment.entityId)
                    for ((index, slot) in equipment.slots.withIndex()) {
                        val last = index == equipment.slots.size - 1
                        val marker = if (last) slot.type.ordinal else slot.type.ordinal or CONTINUES
                        output.writeByte(marker.toByte())
                        writeOldStack(output, slot.stack)
                    }
                }
            }
        }
        return true
    }

    override fun serverbound(
        state: ProtocolState,
        packet: MutablePacket,
    ): Boolean {
        if (state != ProtocolState.PLAY || packet.nativeId != ServerboundSetCreativeModeSlotPacket.ID) return true
        // The stack coming back is the old shape, and what reads it next is the new one.
        val input = decode(packet) { it }
        val slot = input.readShort()
        val present = input.readByte().toInt() != 0
        if (!present) {
            encode(packet) { output ->
                output.writeShort(slot)
                output.writeVarInt(0)
            }
            return true
        }
        val itemId = input.readVarInt()
        val count = input.readByte().toInt()
        val tag = input.readAnonymousNbt()
        encode(packet) { output ->
            output.writeShort(slot)
            output.writeVarInt(count)
            output.writeVarInt(itemId)
            // The tag is dropped rather than guessed at: what it held is components over here, and
            // the names do not line up. What the item is and how many survive.
            output.writeVarInt(0)
            output.writeVarInt(0)
        }
        return true
    }

    /** A stack as everything before 1.20.5 reads one: present, then the item, the count and a tag. */
    private fun writeOldStack(
        output: ByteBuffer,
        item: WireItemStack?,
    ) {
        if (item == null || item.count <= 0) {
            output.writeBoolean(false)
            return
        }
        output.writeBoolean(true)
        output.writeVarInt(item.itemId)
        output.writeByte(item.count.toByte())
        val tag = tagOf(item)
        if (tag == null) {
            // No tag at all, which is a single zero byte rather than an empty compound.
            output.writeByte(0)
        } else {
            output.writeAnonymousNbt(tag)
        }
    }

    /** The two components an old tag has a name for, and null when the stack carries neither. */
    private fun tagOf(item: WireItemStack): NbtCompound? {
        var tag: NbtCompound? = null
        for (component in item.components) {
            when {
                component is ItemComponent.Number && component.id == ItemComponent.DAMAGE -> {
                    val into = tag ?: NbtCompound().also { tag = it }
                    into.putInt("Damage", component.value)
                }
                component is ItemComponent.Text && component.id == ItemComponent.CUSTOM_NAME -> {
                    val into = tag ?: NbtCompound().also { tag = it }
                    // A name travelled as a string of json before it was a component of its own.
                    val display = NbtCompound()
                    display.putString("Name", ComponentJson.serialize(component.text).toString())
                    into["display"] = display
                }
            }
        }
        return tag
    }

    /** An attribute is a number from 1.21 and a name below it, and the name is the far end's. */
    private fun nameAttributes(packet: MutablePacket) {
        val input = decode(packet) { it }
        val entityId = input.readVarInt()
        val attributes =
            List(input.readVarInt()) {
                val id = input.readVarInt()
                val base = input.readDouble()
                val modifiers =
                    List(input.readVarInt()) { Triple(input.readUuid(), input.readDouble(), input.readByte()) }
                Triple(id, base, modifiers)
            }
        encode(packet) { output ->
            output.writeVarInt(entityId)
            val named =
                attributes.mapNotNull { (id, base, modifiers) ->
                    val name = target.attributeNames.name(id) ?: return@mapNotNull null
                    Triple(name, base, modifiers)
                }
            output.writeVarInt(named.size)
            for ((name, base, modifiers) in named) {
                output.writeProtocolString(name)
                output.writeDouble(base)
                output.writeVarInt(modifiers.size)
                for ((uuid, amount, operation) in modifiers) {
                    output.writeUuid(uuid)
                    output.writeDouble(amount)
                    output.writeByte(operation)
                }
            }
        }
    }

    private companion object {
        /** The high bit of an equipment slot, saying another one follows. */
        const val CONTINUES = 0x80
    }
}
