package org.basaltmc.wire.protocol

import org.basaltmc.wire.io.ByteBuffer

/**
 * Mirrors vanilla's `net.minecraft.network.VarInt`: LEB128-style, 7 data bits per byte, MSB as
 * the continuation flag, no zigzag (negative values always take the full 5 bytes). Decompiled
 * from the 26.2 server jar to confirm the exact algorithm rather than assumed from memory.
 */
public object VarInt {
    public const val MAX_BYTES: Int = 5

    public fun sizeOf(value: Int): Int {
        for (i in 1 until 5) {
            if ((value and (-1 shl (i * 7))) == 0) return i
        }
        return 5
    }

    public fun write(
        buffer: ByteBuffer,
        initialValue: Int,
    ) {
        var value = initialValue
        while ((value and -128) != 0) {
            buffer.writeByte(((value and 127) or 128).toByte())
            value = value ushr 7
        }
        buffer.writeByte(value.toByte())
    }

    public fun read(buffer: ByteBuffer): Int {
        var result = 0
        var bytes = 0
        var current: Byte
        do {
            current = buffer.readByte()
            result = result or ((current.toInt() and 127) shl (bytes++ * 7))
            if (bytes > MAX_BYTES) throw ProtocolException("VarInt too big")
        } while ((current.toInt() and 128) == 128)
        return result
    }
}

/** Thrown for malformed protocol input: bad VarInt/VarLong, oversized strings, unknown packet IDs. */
public class ProtocolException(
    message: String,
) : Exception(message)
