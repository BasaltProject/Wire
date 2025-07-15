package org.basaltmc.wire.protocol.play

import org.basaltmc.wire.io.ByteBuffer
import org.basaltmc.wire.protocol.readBoolean
import org.basaltmc.wire.protocol.readProtocolString
import org.basaltmc.wire.protocol.readVarInt
import org.basaltmc.wire.protocol.readVarLong
import org.basaltmc.wire.protocol.writeBoolean
import org.basaltmc.wire.protocol.writeProtocolString
import org.basaltmc.wire.protocol.writeVarInt
import org.basaltmc.wire.protocol.writeVarLong

/**
 * Clientbound, id 0x2b: the whole world border at once, which is what a joining client needs.
 * Mirrors `ClientboundInitializeBorderPacket`.
 */
public data class InitializeBorderPacket(
    val centerX: Double,
    val centerZ: Double,
    val oldSize: Double,
    val newSize: Double,
    val lerpTimeTicks: Long,
    val absoluteMaxSize: Int,
    val warningBlocks: Int,
    val warningSeconds: Int,
) {
    public fun write(buffer: ByteBuffer) {
        buffer.writeDouble(centerX)
        buffer.writeDouble(centerZ)
        buffer.writeDouble(oldSize)
        buffer.writeDouble(newSize)
        buffer.writeVarLong(lerpTimeTicks)
        buffer.writeVarInt(absoluteMaxSize)
        buffer.writeVarInt(warningBlocks)
        buffer.writeVarInt(warningSeconds)
    }

    public companion object {
        public const val ID: Int = 0x2b

        /** parity: `WorldBorder.ABSOLUTE_MAX_SIZE`, which is what vanilla always sends here. */
        public const val ABSOLUTE_MAX_SIZE: Int = 29999984

        public fun read(buffer: ByteBuffer): InitializeBorderPacket =
            InitializeBorderPacket(
                centerX = buffer.readDouble(),
                centerZ = buffer.readDouble(),
                oldSize = buffer.readDouble(),
                newSize = buffer.readDouble(),
                lerpTimeTicks = buffer.readVarLong(),
                absoluteMaxSize = buffer.readVarInt(),
                warningBlocks = buffer.readVarInt(),
                warningSeconds = buffer.readVarInt(),
            )
    }
}

/** Clientbound, id 0x58: where the border is centred. */
public data class SetBorderCenterPacket(
    val centerX: Double,
    val centerZ: Double,
) {
    public fun write(buffer: ByteBuffer) {
        buffer.writeDouble(centerX)
        buffer.writeDouble(centerZ)
    }

    public companion object {
        public const val ID: Int = 0x58

        public fun read(buffer: ByteBuffer): SetBorderCenterPacket =
            SetBorderCenterPacket(buffer.readDouble(), buffer.readDouble())
    }
}

/** Clientbound, id 0x5a: the border's size, changed at once. */
public data class SetBorderSizePacket(
    val size: Double,
) {
    public fun write(buffer: ByteBuffer) {
        buffer.writeDouble(size)
    }

    public companion object {
        public const val ID: Int = 0x5a

        public fun read(buffer: ByteBuffer): SetBorderSizePacket = SetBorderSizePacket(buffer.readDouble())
    }
}

/** Clientbound, id 0x59: the border's size, moved to over [lerpTimeTicks]. */
public data class SetBorderLerpSizePacket(
    val oldSize: Double,
    val newSize: Double,
    val lerpTimeTicks: Long,
) {
    public fun write(buffer: ByteBuffer) {
        buffer.writeDouble(oldSize)
        buffer.writeDouble(newSize)
        buffer.writeVarLong(lerpTimeTicks)
    }

    public companion object {
        public const val ID: Int = 0x59

        public fun read(buffer: ByteBuffer): SetBorderLerpSizePacket =
            SetBorderLerpSizePacket(buffer.readDouble(), buffer.readDouble(), buffer.readVarLong())
    }
}

/** Clientbound, id 0x5b: how many seconds before a shrinking border the warning appears. */
public data class SetBorderWarningDelayPacket(
    val warningSeconds: Int,
) {
    public fun write(buffer: ByteBuffer) {
        buffer.writeVarInt(warningSeconds)
    }

    public companion object {
        public const val ID: Int = 0x5b

        public fun read(buffer: ByteBuffer): SetBorderWarningDelayPacket =
            SetBorderWarningDelayPacket(buffer.readVarInt())
    }
}

/** Clientbound, id 0x5c: how close to the border the warning appears. */
public data class SetBorderWarningDistancePacket(
    val warningBlocks: Int,
) {
    public fun write(buffer: ByteBuffer) {
        buffer.writeVarInt(warningBlocks)
    }

    public companion object {
        public const val ID: Int = 0x5c

        public fun read(buffer: ByteBuffer): SetBorderWarningDistancePacket =
            SetBorderWarningDistancePacket(buffer.readVarInt())
    }
}

/** Clientbound, id 0x5d: whose eyes the client is looking through. Its own id puts it back. */
public data class SetCameraPacket(
    val cameraEntityId: Int,
) {
    public fun write(buffer: ByteBuffer) {
        buffer.writeVarInt(cameraEntityId)
    }

    public companion object {
        public const val ID: Int = 0x5d

        public fun read(buffer: ByteBuffer): SetCameraPacket = SetCameraPacket(buffer.readVarInt())
    }
}

/** Clientbound, id 0x7f: the tick rate the server is running at, and whether it is frozen. */
public data class TickingStatePacket(
    val tickRate: Float,
    val frozen: Boolean,
) {
    public fun write(buffer: ByteBuffer) {
        buffer.writeFloat(tickRate)
        buffer.writeBoolean(frozen)
    }

    public companion object {
        public const val ID: Int = 0x7f

        public fun read(buffer: ByteBuffer): TickingStatePacket =
            TickingStatePacket(buffer.readFloat(), buffer.readBoolean())
    }
}

/** Clientbound, id 0x80: how many ticks a frozen server is about to step through. */
public data class TickingStepPacket(
    val steps: Int,
) {
    public fun write(buffer: ByteBuffer) {
        buffer.writeVarInt(steps)
    }

    public companion object {
        public const val ID: Int = 0x80

        public fun read(buffer: ByteBuffer): TickingStepPacket = TickingStepPacket(buffer.readVarInt())
    }
}

/**
 * Clientbound, id 0x81: tells the client to reconnect somewhere else, keeping its cookies. Mirrors
 * `ClientboundTransferPacket`.
 */
public data class TransferPacket(
    val host: String,
    val port: Int,
) {
    public fun write(buffer: ByteBuffer) {
        buffer.writeProtocolString(host)
        buffer.writeVarInt(port)
    }

    public companion object {
        public const val ID: Int = 0x81

        public fun read(buffer: ByteBuffer): TransferPacket =
            TransferPacket(buffer.readProtocolString(), buffer.readVarInt())
    }
}
