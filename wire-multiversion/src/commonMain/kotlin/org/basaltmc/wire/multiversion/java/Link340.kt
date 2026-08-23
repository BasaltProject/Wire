package org.basaltmc.wire.multiversion.java

import org.basaltmc.wire.io.ByteBuffer
import org.basaltmc.wire.multiversion.MutablePacket
import org.basaltmc.wire.multiversion.VersionTables
import org.basaltmc.wire.protocol.ProtocolState
import org.basaltmc.wire.protocol.play.AwardStatsPacket
import org.basaltmc.wire.protocol.play.ContainerSetContentPacket
import org.basaltmc.wire.protocol.play.ContainerSetSlotPacket
import org.basaltmc.wire.protocol.play.LevelParticlesPacket
import org.basaltmc.wire.protocol.play.MapChunkPacket
import org.basaltmc.wire.protocol.play.ServerboundContainerClickPacket
import org.basaltmc.wire.protocol.play.ServerboundSetCreativeModeSlotPacket
import org.basaltmc.wire.protocol.play.SetEquipmentPacket
import org.basaltmc.wire.protocol.readAnonymousNbt
import org.basaltmc.wire.protocol.readVarInt
import org.basaltmc.wire.protocol.writeAnonymousNbt
import org.basaltmc.wire.protocol.writeBoolean
import org.basaltmc.wire.protocol.writeVarInt

/**
 * 1.13 to 1.12.2, the flattening.
 *
 * A block is an id and four bits of metadata below 1.13, and an item an id and a damage. The
 * numbers are the tables' work; the shape of a stack is this one's.
 *
 * Dropped rather than sent wrong: particles, statistics and maps, whose payloads carry no meaning
 * across.
 */
internal class Link340(
    tables: VersionTables,
) : RenumberingLink(tables) {
    override fun clientbound(
        state: ProtocolState,
        packet: MutablePacket,
    ): Boolean {
        if (state != ProtocolState.PLAY) return true
        when (packet.nativeId) {
            LevelParticlesPacket.ID, AwardStatsPacket.ID -> return false
            MapChunkPacket.ID -> narrowBiomes(packet)
            ContainerSetSlotPacket.ID -> {
                val input = decode(packet) { it }
                val container = input.readByte()
                val slot = input.readShort()
                val stack = oldStack(input)
                encode(packet) { output ->
                    output.writeByte(container)
                    output.writeShort(slot)
                    output.writeBytes(stack)
                }
            }
            ContainerSetContentPacket.ID -> {
                val input = decode(packet) { it }
                val container = input.readByte()
                val count = input.readShort()
                val stacks = ByteBuffer()
                repeat(count.toInt()) { stacks.writeBytes(oldStack(input)) }
                val items = stacks.toByteArray()
                encode(packet) { output ->
                    output.writeByte(container)
                    output.writeShort(count)
                    output.writeBytes(items)
                }
            }
            SetEquipmentPacket.ID -> {
                val input = decode(packet) { it }
                val entityId = input.readVarInt()
                val slot = input.readVarInt()
                val stack = oldStack(input)
                encode(packet) { output ->
                    output.writeVarInt(entityId)
                    output.writeVarInt(slot)
                    output.writeBytes(stack)
                }
            }
        }
        return true
    }

    override fun serverbound(
        state: ProtocolState,
        packet: MutablePacket,
    ): Boolean {
        if (state != ProtocolState.PLAY) return true
        when (packet.nativeId) {
            ServerboundSetCreativeModeSlotPacket.ID -> {
                val input = decode(packet) { it }
                val slot = input.readShort()
                val stack = newStack(input)
                encode(packet) { output ->
                    output.writeShort(slot)
                    output.writeBytes(stack)
                }
            }
            ServerboundContainerClickPacket.ID -> {
                // 1.12 numbers its clicks itself and sends the stack it thinks it took. This server
                // wants neither: it wants to know whether the click left something on the cursor.
                val input = decode(packet) { it }
                val container = input.readByte()
                val slot = input.readShort()
                val button = input.readByte()
                input.readShort()
                val mode = input.readByte().toInt()
                val carried = input.readShort().toInt() != EMPTY_ITEM
                encode(packet) { output ->
                    output.writeByte(container)
                    output.writeVarInt(0)
                    output.writeShort(slot)
                    output.writeByte(button)
                    output.writeVarInt(mode)
                    output.writeVarInt(0)
                    output.writeBoolean(carried)
                }
            }
        }
        return true
    }

    /**
     * The biomes of a chunk, which are one byte each below 1.13.
     *
     * They are the tail of the data buffer, so the sections do not have to be walked to find them:
     * a full chunk ends with two hundred and fifty six of them and nothing else.
     */
    private fun narrowBiomes(packet: MutablePacket) {
        val input = decode(packet) { it }
        val x = input.readInt()
        val z = input.readInt()
        val fullChunk = input.readByte()
        val mask = input.readVarInt()
        val data = input.readBytes(input.readVarInt())
        val rest = input.readBytes(input.readableBytes)
        if (fullChunk.toInt() == 0 || data.size < COLUMNS * Int.SIZE_BYTES) return
        val sections = data.copyOfRange(0, data.size - COLUMNS * Int.SIZE_BYTES)
        val biomes = ByteBuffer().apply { writeBytes(data, offset = data.size - COLUMNS * Int.SIZE_BYTES) }
        encode(packet) { output ->
            output.writeInt(x)
            output.writeInt(z)
            output.writeByte(fullChunk)
            output.writeVarInt(mask)
            output.writeVarInt(sections.size + COLUMNS)
            output.writeBytes(sections)
            repeat(COLUMNS) { output.writeByte(biomes.readInt().toByte()) }
            output.writeBytes(rest)
        }
    }

    /** A stack as 1.12.2 reads one: the item in a short, then the count, the damage and its tag. */
    private fun oldStack(input: ByteBuffer): ByteArray {
        val output = ByteBuffer()
        if (input.readByte().toInt() == 0) {
            output.writeShort(EMPTY_ITEM.toShort())
            return output.toByteArray()
        }
        val packed = input.readVarInt()
        val count = input.readByte()
        val tag = input.readAnonymousNbt()
        output.writeShort((packed and ITEM_MASK).toShort())
        output.writeByte(count)
        output.writeShort((packed ushr DAMAGE_SHIFT).toShort())
        output.writeAnonymousNbt(tag)
        return output.toByteArray()
    }

    /** And back: what the client sends is an id and a damage, and above this it is a name's id. */
    private fun newStack(input: ByteBuffer): ByteArray {
        val output = ByteBuffer()
        val itemId = input.readShort().toInt()
        if (itemId == EMPTY_ITEM) {
            output.writeBoolean(false)
            return output.toByteArray()
        }
        val count = input.readByte()
        val damage = input.readShort().toInt()
        val tag = input.readAnonymousNbt()
        // Put back together the way the table holds it: what is renumbered above this reads it.
        output.writeBoolean(true)
        output.writeVarInt(damage shl DAMAGE_SHIFT or itemId)
        output.writeByte(count)
        output.writeAnonymousNbt(tag)
        return output.toByteArray()
    }

    private companion object {
        /** parity: an empty slot is the item id minus one, and nothing follows it. */
        const val EMPTY_ITEM = -1

        const val COLUMNS = 16 * 16

        /** How the tables carry an item and the damage that says which variant of it this is. */
        const val ITEM_MASK = 0xFFFF
        const val DAMAGE_SHIFT = 16
    }
}
