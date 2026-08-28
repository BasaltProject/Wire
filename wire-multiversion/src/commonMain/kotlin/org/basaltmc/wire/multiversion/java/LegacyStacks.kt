package org.basaltmc.wire.multiversion.java

import org.basaltmc.wire.io.ByteBuffer
import org.basaltmc.wire.multiversion.ProtocolVersions
import org.basaltmc.wire.nbt.NbtCompound
import org.basaltmc.wire.protocol.play.ItemComponent
import org.basaltmc.wire.protocol.play.WireItemStack
import org.basaltmc.wire.protocol.writeAnonymousNbt
import org.basaltmc.wire.protocol.writeBoolean
import org.basaltmc.wire.protocol.writeVarInt
import org.basaltmc.wire.text.ComponentJson

/**
 * A stack in the shape its own version reads one: components from 1.20.5, an item, count and tag
 * below that, and a short with its damage below 1.13. The id is already the far end's by then.
 */
internal object LegacyStacks {
    fun write(
        output: ByteBuffer,
        item: WireItemStack?,
        farEnd: Int,
    ) {
        when {
            farEnd >= ProtocolVersions.V1_20_5 -> WireItemStack.write(output, item)
            farEnd >= ProtocolVersions.V1_13 -> writeTagged(output, item)
            else -> writeDamaged(output, item)
        }
    }

    /** 1.13 to 1.20.4: present, then the item, the count and a tag. */
    private fun writeTagged(
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
        // No tag at all, which is a single zero byte rather than an empty compound.
        if (tag == null) output.writeByte(0) else output.writeAnonymousNbt(tag)
    }

    /** 1.12.2 and below: the item in a short, then the count, the damage, and the tag. */
    private fun writeDamaged(
        output: ByteBuffer,
        item: WireItemStack?,
    ) {
        if (item == null || item.count <= 0) {
            output.writeShort(EMPTY_ITEM.toShort())
            return
        }
        // The damage rides in the high half of the id: below 1.13 a variant is an item and a number.
        output.writeShort((item.itemId and ITEM_MASK).toShort())
        output.writeByte(item.count.toByte())
        output.writeShort((item.itemId ushr DAMAGE_SHIFT).toShort())
        val tag = tagOf(item)
        if (tag == null) output.writeByte(0) else output.writeAnonymousNbt(tag)
    }

    /** The two components an old tag has a name for, and null when the stack carries neither. */
    fun tagOf(item: WireItemStack): NbtCompound? {
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

    private const val EMPTY_ITEM = -1
    private const val ITEM_MASK = 0xFFFF
    private const val DAMAGE_SHIFT = 16
}
