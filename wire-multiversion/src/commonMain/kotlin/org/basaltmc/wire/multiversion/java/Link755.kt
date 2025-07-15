package org.basaltmc.wire.multiversion.java

import org.basaltmc.wire.io.ByteBuffer
import org.basaltmc.wire.multiversion.MutablePacket
import org.basaltmc.wire.multiversion.VersionTables
import org.basaltmc.wire.protocol.ProtocolState
import org.basaltmc.wire.protocol.play.ContainerSetContentPacket
import org.basaltmc.wire.protocol.play.ContainerSetSlotPacket
import org.basaltmc.wire.protocol.play.RemoveEntitiesPacket
import org.basaltmc.wire.protocol.readAnonymousNbt
import org.basaltmc.wire.protocol.readVarInt
import org.basaltmc.wire.protocol.writeAnonymousNbt
import org.basaltmc.wire.protocol.writeVarInt

/**
 * 1.17.1 to 1.17, the last step of the range.
 *
 * 1.17.1 gathered the entities being removed into one packet and gave every inventory update a
 * state id. 1.17 has neither: entities go one packet each, and the state id has nowhere to sit.
 * The contents of a window also count their slots in a short there rather than as a varint, and
 * what is on the cursor travels as a slot update of its own.
 */
internal class Link755(
    tables: VersionTables,
) : RenumberingLink(tables) {
    override fun clientbound(
        state: ProtocolState,
        packet: MutablePacket,
    ): Boolean {
        if (state != ProtocolState.PLAY) return true
        when (packet.nativeId) {
            RemoveEntitiesPacket.ID -> {
                val input = decode(packet) { it }
                val ids = IntArray(input.readVarInt()) { input.readVarInt() }
                if (ids.isEmpty()) return false
                packet.overrideId(REMOVE_ENTITY_755)
                encode(packet) { it.writeVarInt(ids[0]) }
                for (i in 1 until ids.size) {
                    packet.extras.add(REMOVE_ENTITY_755 to ByteBuffer().apply { writeVarInt(ids[i]) }.toByteArray())
                }
            }
            ContainerSetSlotPacket.ID -> {
                val input = decode(packet) { it }
                val containerId = input.readByte()
                input.readVarInt() // The state id, which 1.17 does not have.
                val rest = input.readBytes(input.readableBytes)
                encode(packet) { output ->
                    output.writeByte(containerId)
                    output.writeBytes(rest)
                }
            }
            ContainerSetContentPacket.ID -> {
                val input = decode(packet) { it }
                val containerId = input.readByte()
                input.readVarInt() // The state id again.
                val count = input.readVarInt()
                val items = ByteBuffer()
                repeat(count) { copyStack(input, items) }
                val slots = items.toByteArray()
                encode(packet) { output ->
                    output.writeByte(containerId)
                    output.writeShort(count.toShort())
                    output.writeBytes(slots)
                }
                // What is on the cursor is the last stack of all, and 1.17 has no field for it.
            }
        }
        return true
    }

    /** One stack as everything below 1.20.5 writes it, copied across without looking inside. */
    private fun copyStack(
        input: ByteBuffer,
        output: ByteBuffer,
    ) {
        val present = input.readByte()
        output.writeByte(present)
        if (present.toInt() == 0) return
        output.writeVarInt(input.readVarInt())
        output.writeByte(input.readByte())
        output.writeAnonymousNbt(input.readAnonymousNbt())
    }

    private companion object {
        const val REMOVE_ENTITY_755 = 58
    }
}
