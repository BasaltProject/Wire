package org.basaltmc.wire.multiversion.java

import org.basaltmc.wire.multiversion.MutablePacket
import org.basaltmc.wire.multiversion.VersionTables
import org.basaltmc.wire.protocol.ProtocolState
import org.basaltmc.wire.protocol.play.SoundPacket
import org.basaltmc.wire.protocol.readProtocolString
import org.basaltmc.wire.protocol.readVarInt
import org.basaltmc.wire.protocol.writeProtocolString
import org.basaltmc.wire.protocol.writeVarInt

/**
 * 1.10.2 to 1.9.4, where the pitch of a sound is one byte.
 *
 * 1.10 made it a float. Below that it is the same number times sixty three, rounded, which is
 * exactly what the client divides it back by.
 */
internal class Link110(
    tables: VersionTables,
) : RenumberingLink(tables) {
    override fun clientbound(
        state: ProtocolState,
        packet: MutablePacket,
    ): Boolean {
        if (state != ProtocolState.PLAY || packet.nativeId != SoundPacket.ID) return true
        val input = decode(packet) { it }
        val named = packet.id == NAMED_SOUND
        val name = if (named) input.readProtocolString() else null
        val sound = if (named) 0 else input.readVarInt()
        val category = input.readVarInt()
        val x = input.readInt()
        val y = input.readInt()
        val z = input.readInt()
        val volume = input.readFloat()
        val pitch = input.readFloat()
        encode(packet) { output ->
            if (name != null) output.writeProtocolString(name) else output.writeVarInt(sound)
            output.writeVarInt(category)
            output.writeInt(x)
            output.writeInt(y)
            output.writeInt(z)
            output.writeFloat(volume)
            output.writeByte((pitch * PITCH_STEPS).toInt().coerceIn(0, 255).toByte())
        }
        return true
    }

    private companion object {
        /** The sound with a name of its own, as 1.10.2 numbers it. */
        const val NAMED_SOUND = 0x19

        /** parity: `ClientboundSoundPacket` wrote the pitch as sixty three steps of one. */
        const val PITCH_STEPS = 63f
    }
}
