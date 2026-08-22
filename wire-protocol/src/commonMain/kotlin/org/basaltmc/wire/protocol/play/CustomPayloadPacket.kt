package org.basaltmc.wire.protocol.play

import org.basaltmc.wire.io.ByteBuffer
import org.basaltmc.wire.protocol.readProtocolString
import org.basaltmc.wire.protocol.writeProtocolString

/**
 * A message on a named channel, in either direction. Mirrors `ClientboundCustomPayloadPacket` and
 * its serverbound twin.
 *
 * The body has no length prefix of its own: whatever is left in the frame after the channel name is
 * the payload, which is why this reads to the end rather than to a count. That is also what makes it
 * usable for anything, since the server is not the one deciding what the bytes mean.
 */
public data class CustomPayloadPacket(
    val channel: String,
    val data: ByteArray,
) {
    public fun write(buffer: ByteBuffer) {
        buffer.writeProtocolString(channel)
        buffer.writeBytes(data)
    }

    /** By value, because the payload is the packet: two with the same bytes are the same message. */
    override fun equals(other: Any?): Boolean =
        other is CustomPayloadPacket && channel == other.channel && data.contentEquals(other.data)

    override fun hashCode(): Int = 31 * channel.hashCode() + data.contentHashCode()

    public companion object {
        public const val CLIENTBOUND_ID: Int = 0x18
        public const val SERVERBOUND_ID: Int = 0x16

        /** The one vanilla sends itself, and the reason a server sees a payload before any plugin does. */
        public const val BRAND_CHANNEL: String = "minecraft:brand"

        public fun read(buffer: ByteBuffer): CustomPayloadPacket {
            val channel = buffer.readProtocolString()
            return CustomPayloadPacket(channel, buffer.readBytes(buffer.readableBytes))
        }
    }
}
