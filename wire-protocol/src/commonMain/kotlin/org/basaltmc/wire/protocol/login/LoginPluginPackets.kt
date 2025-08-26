package org.basaltmc.wire.protocol.login

import org.basaltmc.wire.io.ByteBuffer
import org.basaltmc.wire.protocol.readBoolean
import org.basaltmc.wire.protocol.readProtocolString
import org.basaltmc.wire.protocol.readVarInt
import org.basaltmc.wire.protocol.writeBoolean
import org.basaltmc.wire.protocol.writeProtocolString
import org.basaltmc.wire.protocol.writeVarInt

/** The most a login query payload may carry, both ways. // parity: `ClientboundCustomQueryPacket.MAX_PAYLOAD_SIZE`. */
private const val MAX_PAYLOAD_SIZE = 1048576

/**
 * Clientbound, id 0x04: an out-of-band question during login, mirroring `ClientboundCustomQueryPacket`.
 * The payload is written raw and runs to the end of the frame.
 */
public data class LoginPluginRequestPacket(
    val transactionId: Int,
    val channel: String,
    val payload: ByteArray,
) {
    public fun write(buffer: ByteBuffer) {
        buffer.writeVarInt(transactionId)
        buffer.writeProtocolString(channel)
        buffer.writeBytes(payload)
    }

    override fun equals(other: Any?): Boolean =
        other is LoginPluginRequestPacket &&
            transactionId == other.transactionId &&
            channel == other.channel &&
            payload.contentEquals(other.payload)

    override fun hashCode(): Int {
        var result = transactionId
        result = 31 * result + channel.hashCode()
        result = 31 * result + payload.contentHashCode()
        return result
    }

    public companion object {
        public const val ID: Int = 4

        public fun read(buffer: ByteBuffer): LoginPluginRequestPacket {
            val transactionId = buffer.readVarInt()
            val channel = buffer.readProtocolString()
            val payload = buffer.readBytes(buffer.readableBytes.coerceAtMost(MAX_PAYLOAD_SIZE))
            return LoginPluginRequestPacket(transactionId, channel, payload)
        }
    }
}

/**
 * Serverbound, id 0x02: the answer to [LoginPluginRequestPacket], matched by transaction id.
 * A null [payload] is "I do not know that channel", which is what every real client answers.
 */
public data class LoginPluginResponsePacket(
    val transactionId: Int,
    val payload: ByteArray?,
) {
    public fun write(buffer: ByteBuffer) {
        buffer.writeVarInt(transactionId)
        buffer.writeBoolean(payload != null)
        payload?.let { buffer.writeBytes(it) }
    }

    override fun equals(other: Any?): Boolean =
        other is LoginPluginResponsePacket &&
            transactionId == other.transactionId &&
            payload.contentEqualsNullable(other.payload)

    override fun hashCode(): Int = 31 * transactionId + (payload?.contentHashCode() ?: 0)

    public companion object {
        public const val ID: Int = 2

        public fun read(buffer: ByteBuffer): LoginPluginResponsePacket {
            val transactionId = buffer.readVarInt()
            val present = buffer.readBoolean()
            val payload =
                if (present) buffer.readBytes(buffer.readableBytes.coerceAtMost(MAX_PAYLOAD_SIZE)) else null
            return LoginPluginResponsePacket(transactionId, payload)
        }
    }
}

private fun ByteArray?.contentEqualsNullable(other: ByteArray?): Boolean =
    if (this == null || other == null) this === other else contentEquals(other)
