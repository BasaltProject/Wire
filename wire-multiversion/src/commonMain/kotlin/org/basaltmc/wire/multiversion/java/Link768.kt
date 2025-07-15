package org.basaltmc.wire.multiversion.java

import org.basaltmc.wire.io.ByteBuffer
import org.basaltmc.wire.multiversion.MutablePacket
import org.basaltmc.wire.multiversion.VersionTables
import org.basaltmc.wire.protocol.ProtocolState
import org.basaltmc.wire.protocol.play.LevelParticlesPacket
import org.basaltmc.wire.protocol.play.SetHeldSlotPacket
import org.basaltmc.wire.protocol.readBoolean
import org.basaltmc.wire.protocol.writeBoolean

/**
 * 1.21.4 to 1.21.3. Two fields, both small.
 *
 * The slot in the hand became a variable-length number in 1.21.4 and used to be a single byte, and
 * a particle gained the flag that shows it however far away it is.
 */
internal class Link768(
    tables: VersionTables,
) : RenumberingLink(tables) {
    override fun clientbound(
        state: ProtocolState,
        packet: MutablePacket,
    ): Boolean {
        if (!super.clientbound(state, packet)) return false
        if (state != ProtocolState.PLAY) return true
        when (packet.nativeId) {
            SetHeldSlotPacket.ID -> {
                val slot = decode(packet) { SetHeldSlotPacket.read(it) }
                encode(packet) { it.writeByte(slot.slot.toByte()) }
            }
            LevelParticlesPacket.ID -> {
                // Only the second field goes; the rest of the packet is the same bytes, so it is
                // copied across rather than decoded, which keeps the particle's own options out of
                // this link's business.
                val input = ByteBuffer().apply { writeBytes(packet.body) }
                val overrideLimiter = input.readBoolean()
                input.readBoolean()
                encode(packet) { output ->
                    output.writeBoolean(overrideLimiter)
                    output.writeBytes(input.readBytes(input.readableBytes))
                }
            }
        }
        return true
    }
}
