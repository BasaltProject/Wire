package org.basaltmc.wire.multiversion.java

import org.basaltmc.wire.multiversion.MutablePacket
import org.basaltmc.wire.multiversion.VersionTables
import org.basaltmc.wire.protocol.ProtocolState
import org.basaltmc.wire.protocol.play.ClearTitlesPacket
import org.basaltmc.wire.protocol.play.ClientboundTakeItemEntityPacket
import org.basaltmc.wire.protocol.play.SetActionBarTextPacket
import org.basaltmc.wire.protocol.play.SetSubtitleTextPacket
import org.basaltmc.wire.protocol.play.SetTitleTextPacket
import org.basaltmc.wire.protocol.play.SetTitlesAnimationPacket
import org.basaltmc.wire.protocol.play.SpawnEntityPacket
import org.basaltmc.wire.protocol.readVarInt
import org.basaltmc.wire.protocol.writeVarInt

/**
 * 1.11 to 1.10.2, where a mob is one byte and there is no line above the hotbar.
 *
 * 1.11 gave the kind of a mob a varint, added the action bar to the title packet and started
 * saying how many items were picked up. A client below it has one byte for the mob, four actions
 * where 1.11 has six, and no field for the count.
 */
internal class Link210(
    tables: VersionTables,
) : RenumberingLink(tables) {
    override fun clientbound(
        state: ProtocolState,
        packet: MutablePacket,
    ): Boolean {
        if (state != ProtocolState.PLAY) return true
        when (packet.nativeId) {
            SpawnEntityPacket.ID -> {
                // Only a mob is affected: the link to 1.18.2 gave it the packet 1.11 calls its own.
                if (packet.id != ADD_MOB) return true
                val input = decode(packet) { it }
                val entityId = input.readVarInt()
                val mostSignificant = input.readLong()
                val leastSignificant = input.readLong()
                val type = input.readVarInt()
                val rest = input.readBytes(input.readableBytes)
                encode(packet) { output ->
                    output.writeVarInt(entityId)
                    output.writeLong(mostSignificant)
                    output.writeLong(leastSignificant)
                    output.writeByte(type.toByte())
                    output.writeBytes(rest)
                }
            }
            ClientboundTakeItemEntityPacket.ID -> {
                val input = decode(packet) { it }
                val collected = input.readVarInt()
                val collector = input.readVarInt()
                encode(packet) { output ->
                    output.writeVarInt(collected)
                    output.writeVarInt(collector)
                }
            }
            SetActionBarTextPacket.ID -> return false
            SetTitleTextPacket.ID, SetSubtitleTextPacket.ID,
            SetTitlesAnimationPacket.ID, ClearTitlesPacket.ID,
            -> {
                val input = decode(packet) { it }
                val action = input.readVarInt()
                val rest = input.readBytes(input.readableBytes)
                encode(packet) { output ->
                    output.writeVarInt(if (action > ACTION_BAR) action - 1 else action)
                    output.writeBytes(rest)
                }
            }
        }
        return true
    }

    private companion object {
        /** What the link to 1.16.4 named the mob spawn, in that version's numbering. */
        const val ADD_MOB = 0x03

        /** parity: `ClientboundSetTitlesPacket.Type`, where 1.11 put the action bar second. */
        const val ACTION_BAR = 2
    }
}
