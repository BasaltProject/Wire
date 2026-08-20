package org.basaltmc.wire.protocol.play

import org.basaltmc.wire.io.ByteBuffer
import org.basaltmc.wire.protocol.readProtocolString
import org.basaltmc.wire.protocol.readVarInt
import org.basaltmc.wire.protocol.writeProtocolString
import org.basaltmc.wire.protocol.writeVarInt

/**
 * Clientbound, id 0x16: an item that cannot be used again yet. Mirrors `ClientboundCooldownPacket`.
 *
 * The group is usually the item's own name; what it does on the client is grey the slot out and
 * sweep the little clock over it while the ticks run down.
 */
public data class CooldownPacket(
    val cooldownGroup: String,
    val durationTicks: Int,
) {
    public fun write(buffer: ByteBuffer) {
        buffer.writeProtocolString(cooldownGroup)
        buffer.writeVarInt(durationTicks)
    }

    public companion object {
        public const val ID: Int = 0x16

        public fun read(buffer: ByteBuffer): CooldownPacket =
            CooldownPacket(buffer.readProtocolString(), buffer.readVarInt())
    }
}
