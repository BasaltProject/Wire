package org.basaltmc.protocol.status

import org.basaltmc.io.ByteBuffer
import org.basaltmc.protocol.readProtocolString
import org.basaltmc.protocol.writeProtocolString

/** Serverbound, id 0x00, empty. Mirrors `ServerboundStatusRequestPacket`. */
public object StatusRequestPacket {
    public const val ID: Int = 0
}

/** Clientbound, id 0x00: the Server List Ping JSON body. Mirrors `ClientboundStatusResponsePacket`. */
public data class StatusResponsePacket(
    val status: ServerStatusPayload,
) {
    public fun write(buffer: ByteBuffer) {
        buffer.writeProtocolString(status.toJsonString())
    }

    public companion object {
        public const val ID: Int = 0

        public fun read(buffer: ByteBuffer): StatusResponsePacket =
            StatusResponsePacket(ServerStatusPayload.fromJsonString(buffer.readProtocolString()))
    }
}

/** Serverbound, id 0x01: an opaque timestamp to echo back. Mirrors `ServerboundPingRequestPacket`. */
public data class PingRequestPacket(
    val time: Long,
) {
    public fun write(buffer: ByteBuffer) {
        buffer.writeLong(time)
    }

    public companion object {
        public const val ID: Int = 1

        public fun read(buffer: ByteBuffer): PingRequestPacket = PingRequestPacket(buffer.readLong())
    }
}

/** Clientbound, id 0x01: echoes [PingRequestPacket.time] back. Mirrors `ClientboundPongResponsePacket`. */
public data class PongResponsePacket(
    val time: Long,
) {
    public fun write(buffer: ByteBuffer) {
        buffer.writeLong(time)
    }

    public companion object {
        public const val ID: Int = 1

        public fun read(buffer: ByteBuffer): PongResponsePacket = PongResponsePacket(buffer.readLong())
    }
}
