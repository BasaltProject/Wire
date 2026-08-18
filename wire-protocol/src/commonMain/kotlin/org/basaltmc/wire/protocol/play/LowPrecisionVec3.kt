package org.basaltmc.wire.protocol.play

import org.basaltmc.wire.io.ByteBuffer
import org.basaltmc.wire.protocol.readVarInt
import org.basaltmc.wire.protocol.writeVarInt
import kotlin.math.abs
import kotlin.math.ceil
import kotlin.math.max
import kotlin.math.min
import kotlin.math.round

/**
 * The quantised vector 26.2 introduced for the places a rough direction is enough.
 *
 * Mirrors `LpVec3`. Six bytes carry three fifteen-bit fractions and a scale of up to four; anything
 * bigger sets a continuation bit and writes the rest of the scale as a varint. A vector shorter than
 * the smallest step is one zero byte, which is why the common "nothing" case costs a single byte.
 */
public object LowPrecisionVec3 {
    /** Mirrors the constants of `LpVec3`, which the packing is written in terms of. */
    private const val DATA_MASK = 32767L
    private const val MAX_QUANTIZED = 32766.0
    private const val SCALE_MASK = 3L
    private const val CONTINUATION = 4L
    private const val X_OFFSET = 3
    private const val Y_OFFSET = 18
    private const val Z_OFFSET = 33

    public const val ABS_MAX_VALUE: Double = 1.7179869183E10
    public const val ABS_MIN_VALUE: Double = 3.051944088384301E-5

    public fun read(buffer: ByteBuffer): DoubleArray {
        val lowest = buffer.readByte().toInt() and 0xff
        if (lowest == 0) return doubleArrayOf(0.0, 0.0, 0.0)
        val middle = buffer.readByte().toInt() and 0xff
        val highest = buffer.readInt().toLong() and 0xffffffffL
        val packed = (highest shl 16) or (middle.toLong() shl 8) or lowest.toLong()
        var scale = lowest.toLong() and SCALE_MASK
        if (lowest.toLong() and CONTINUATION == CONTINUATION) {
            scale = scale or ((buffer.readVarInt().toLong() and 0xffffffffL) shl 2)
        }
        return doubleArrayOf(
            unpack(packed shr X_OFFSET) * scale,
            unpack(packed shr Y_OFFSET) * scale,
            unpack(packed shr Z_OFFSET) * scale,
        )
    }

    public fun write(
        buffer: ByteBuffer,
        x: Double,
        y: Double,
        z: Double,
    ) {
        val sx = sanitize(x)
        val sy = sanitize(y)
        val sz = sanitize(z)
        val longest = max(abs(sx), max(abs(sy), abs(sz)))
        if (longest < ABS_MIN_VALUE) {
            buffer.writeByte(0)
            return
        }
        val scale = ceil(longest).toLong()
        val partial = (scale and SCALE_MASK) != scale
        val markers = if (partial) (scale and SCALE_MASK) or CONTINUATION else scale
        val packed =
            markers or
                (pack(sx / scale) shl X_OFFSET) or
                (pack(sy / scale) shl Y_OFFSET) or
                (pack(sz / scale) shl Z_OFFSET)
        buffer.writeByte(packed.toByte())
        buffer.writeByte((packed shr 8).toByte())
        buffer.writeInt((packed shr 16).toInt())
        if (partial) buffer.writeVarInt((scale shr 2).toInt())
    }

    private fun sanitize(value: Double): Double =
        if (value.isNaN()) 0.0 else value.coerceIn(-ABS_MAX_VALUE, ABS_MAX_VALUE)

    private fun pack(value: Double): Long = round((value * 0.5 + 0.5) * MAX_QUANTIZED).toLong()

    private fun unpack(value: Long): Double =
        min((value and DATA_MASK).toDouble(), MAX_QUANTIZED) * 2.0 / MAX_QUANTIZED - 1.0
}
