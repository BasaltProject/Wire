package org.basaltmc.protocol

import org.basaltmc.io.ByteBuffer

/** Mirrors vanilla's `net.minecraft.network.VarLong`: same scheme as [VarInt], up to 10 bytes. */
public object VarLong {
    public const val MAX_BYTES: Int = 10

    public fun write(
        buffer: ByteBuffer,
        initialValue: Long,
    ) {
        var value = initialValue
        while ((value and -128L) != 0L) {
            buffer.writeByte(((value and 127L) or 128L).toByte())
            value = value ushr 7
        }
        buffer.writeByte(value.toByte())
    }

    public fun read(buffer: ByteBuffer): Long {
        var result = 0L
        var bytes = 0
        var current: Byte
        do {
            current = buffer.readByte()
            result = result or ((current.toLong() and 127L) shl (bytes++ * 7))
            if (bytes > MAX_BYTES) throw ProtocolException("VarLong too big")
        } while ((current.toInt() and 128) == 128)
        return result
    }
}
