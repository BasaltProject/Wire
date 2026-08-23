package org.basaltmc.wire.multiversion.java

import org.basaltmc.wire.multiversion.MutablePacket
import org.basaltmc.wire.multiversion.VersionTables
import org.basaltmc.wire.protocol.ProtocolState
import org.basaltmc.wire.protocol.play.LoginPlayPacket

/**
 * 1.9.2 to 1.9, where the dimension fits in one byte.
 *
 * The only difference between the two: 1.9.2 widened it to four so that a mod could have more of
 * them, and the three vanilla has fit either way.
 */
internal class Link107(
    tables: VersionTables,
) : RenumberingLink(tables) {
    override fun clientbound(
        state: ProtocolState,
        packet: MutablePacket,
    ): Boolean {
        if (state != ProtocolState.PLAY || packet.nativeId != LoginPlayPacket.ID) return true
        val input = decode(packet) { it }
        val entityId = input.readInt()
        val gamemode = input.readByte()
        val dimension = input.readInt()
        val rest = input.readBytes(input.readableBytes)
        encode(packet) { output ->
            output.writeInt(entityId)
            output.writeByte(gamemode)
            output.writeByte(dimension.toByte())
            output.writeBytes(rest)
        }
        return true
    }
}
