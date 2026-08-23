package org.basaltmc.wire.multiversion.java

import org.basaltmc.wire.multiversion.MutablePacket
import org.basaltmc.wire.multiversion.VersionTables
import org.basaltmc.wire.protocol.ProtocolState
import org.basaltmc.wire.protocol.login.LoginFinishedPacket
import org.basaltmc.wire.protocol.play.LoginPlayPacket
import org.basaltmc.wire.protocol.readProtocolString
import org.basaltmc.wire.protocol.readVarInt
import org.basaltmc.wire.protocol.writeProtocolString
import org.basaltmc.wire.protocol.writeVarInt

/**
 * 26.2 to 26.1. Nothing in play changed, but the login did.
 *
 * 26.2 put a session id at the end of the packet that finishes logging in, and a flag saying
 * whether the server is in online mode at the end of the one that starts the game. Every version
 * below reads neither, so both have to go: a client left with a packet longer than the one it knows
 * stops at the last thing it reads before the game starts.
 */
internal class Link775(
    tables: VersionTables,
) : RenumberingLink(tables) {
    override fun clientbound(
        state: ProtocolState,
        packet: MutablePacket,
    ): Boolean {
        if (state == ProtocolState.PLAY && packet.nativeId == LoginPlayPacket.ID) {
            val login = decode(packet) { LoginFields.read(it, withOnlineMode = true, withSeaLevel = true) }
            encode(packet) { login.write(it, withOnlineMode = false, withSeaLevel = true) }
            return true
        }
        if (state != ProtocolState.LOGIN || packet.nativeId != LoginFinishedPacket.ID) return true
        rewrite(packet) { input, output ->
            // The profile, field by field: there is no session id on the other side to write.
            output.writeLong(input.readLong())
            output.writeLong(input.readLong())
            output.writeProtocolString(input.readProtocolString(16), 16)
            val properties = input.readVarInt()
            output.writeVarInt(properties)
            repeat(properties) {
                output.writeProtocolString(input.readProtocolString())
                output.writeProtocolString(input.readProtocolString())
                val signed = input.readByte()
                output.writeByte(signed)
                if (signed.toInt() != 0) output.writeProtocolString(input.readProtocolString())
            }
        }
        return true
    }
}
