package org.basaltmc.wire.multiversion.java

import org.basaltmc.wire.multiversion.MutablePacket
import org.basaltmc.wire.multiversion.VersionTables
import org.basaltmc.wire.protocol.ProtocolState
import org.basaltmc.wire.protocol.play.MapChunkPacket
import org.basaltmc.wire.protocol.readVarInt
import org.basaltmc.wire.protocol.writeVarInt

/**
 * 1.9.4 to 1.9.2, where a chunk does not carry its block entities.
 *
 * 1.9.4 added the list at the end of the chunk. Below it a sign or a chest sends its own packet,
 * which this server does anyway, so the list simply goes.
 */
internal class Link109(
    tables: VersionTables,
) : RenumberingLink(tables) {
    override fun clientbound(
        state: ProtocolState,
        packet: MutablePacket,
    ): Boolean {
        if (state != ProtocolState.PLAY || packet.nativeId != MapChunkPacket.ID) return true
        val input = decode(packet) { it }
        val x = input.readInt()
        val z = input.readInt()
        val fullChunk = input.readByte()
        val mask = input.readVarInt()
        val data = input.readBytes(input.readVarInt())
        encode(packet) { output ->
            output.writeInt(x)
            output.writeInt(z)
            output.writeByte(fullChunk)
            output.writeVarInt(mask)
            output.writeVarInt(data.size)
            output.writeBytes(data)
        }
        return true
    }
}
