package org.basaltmc.protocol.handshake

import org.basaltmc.io.ByteBuffer
import org.basaltmc.protocol.ProtocolException
import org.basaltmc.protocol.readProtocolString
import org.basaltmc.protocol.readVarInt
import org.basaltmc.protocol.writeProtocolString
import org.basaltmc.protocol.writeVarInt

/** Mirrors vanilla's `net.minecraft.network.protocol.handshake.ClientIntent`. */
public enum class ClientIntent(
    public val id: Int,
) {
    STATUS(1),
    LOGIN(2),
    TRANSFER(3),
    ;

    public companion object {
        public fun fromId(id: Int): ClientIntent =
            entries.find { it.id == id } ?: throw ProtocolException("Unknown client intent: $id")
    }
}

/**
 * Mirrors vanilla's `net.minecraft.network.protocol.handshake.ClientIntentionPacket` — the only
 * packet in the handshake state, always id 0x00. Field layout confirmed against the decompiled
 * 26.2 server jar.
 */
public data class HandshakePacket(
    val protocolVersion: Int,
    val hostName: String,
    val port: Int,
    val intent: ClientIntent,
) {
    public fun write(buffer: ByteBuffer) {
        buffer.writeVarInt(protocolVersion)
        buffer.writeProtocolString(hostName, MAX_HOST_LENGTH)
        buffer.writeShort(port.toShort())
        buffer.writeVarInt(intent.id)
    }

    public companion object {
        public const val ID: Int = 0
        private const val MAX_HOST_LENGTH = 255

        public fun read(buffer: ByteBuffer): HandshakePacket {
            val protocolVersion = buffer.readVarInt()
            val hostName = buffer.readProtocolString(MAX_HOST_LENGTH)
            val port = buffer.readShort().toInt() and 0xFFFF
            val intent = ClientIntent.fromId(buffer.readVarInt())
            return HandshakePacket(protocolVersion, hostName, port, intent)
        }
    }
}
