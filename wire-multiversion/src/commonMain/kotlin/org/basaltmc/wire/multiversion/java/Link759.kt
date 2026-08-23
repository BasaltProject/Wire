package org.basaltmc.wire.multiversion.java

import org.basaltmc.wire.multiversion.MutablePacket
import org.basaltmc.wire.multiversion.VersionTables
import org.basaltmc.wire.protocol.ProtocolState
import org.basaltmc.wire.protocol.play.SystemChatPacket
import org.basaltmc.wire.protocol.readProtocolString
import org.basaltmc.wire.protocol.writeProtocolString
import org.basaltmc.wire.protocol.writeVarInt

/**
 * 1.19.1 to 1.19. One field, and it is the one that says where a message goes.
 *
 * 1.19.1 turned the kind of a system message into a flag: on the action bar, or not. Before it the
 * kind is a number out of the chat type registry, so the flag becomes the number a client of 1.19
 * looks up.
 */
internal class Link759(
    tables: VersionTables,
) : RenumberingLink(tables) {
    override fun clientbound(
        state: ProtocolState,
        packet: MutablePacket,
    ): Boolean {
        if (state != ProtocolState.PLAY || packet.nativeId != SystemChatPacket.ID) return true
        // The text is already a string of json by here: the link to 1.20.2 put it back.
        val input = decode(packet) { it }
        val json = input.readProtocolString(MAX_TEXT)
        val overlay = input.readByte().toInt() != 0
        encode(packet) { output ->
            output.writeProtocolString(json, MAX_TEXT)
            output.writeVarInt(if (overlay) GAME_INFO else SYSTEM)
        }
        return true
    }

    private companion object {
        /** parity: the ids `ChatType` gives its own entries, system and the one above the hotbar. */
        const val SYSTEM = 1
        const val GAME_INFO = 2
        const val MAX_TEXT = 262144
    }
}
