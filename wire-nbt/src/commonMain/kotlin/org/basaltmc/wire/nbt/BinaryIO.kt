package org.basaltmc.wire.nbt

/**
 * Minimal big-endian byte sink, self-contained so `wire-nbt` does not need to depend on
 * `wire-io`'s network-oriented buffers. Not exposed outside this module.
 */
internal interface ByteWriter {
    fun writeByte(value: Byte)
}

internal interface ByteReader {
    fun readByte(): Byte
}

internal fun ByteWriter.writeShort(value: Short) {
    val v = value.toInt()
    writeByte((v shr 8).toByte())
    writeByte(v.toByte())
}

internal fun ByteWriter.writeInt(value: Int) {
    writeByte((value shr 24).toByte())
    writeByte((value shr 16).toByte())
    writeByte((value shr 8).toByte())
    writeByte(value.toByte())
}

internal fun ByteWriter.writeLong(value: Long) {
    for (shift in 56 downTo 0 step 8) {
        writeByte((value ushr shift).toByte())
    }
}

internal fun ByteWriter.writeFloat(value: Float) {
    writeInt(value.toRawBits())
}

internal fun ByteWriter.writeDouble(value: Double) {
    writeLong(value.toRawBits())
}

internal fun ByteReader.readShort(): Short {
    val a = readByte().toInt() and 0xFF
    val b = readByte().toInt() and 0xFF
    return ((a shl 8) or b).toShort()
}

internal fun ByteReader.readInt(): Int {
    var result = 0
    repeat(4) { result = (result shl 8) or (readByte().toInt() and 0xFF) }
    return result
}

internal fun ByteReader.readLong(): Long {
    var result = 0L
    repeat(8) { result = (result shl 8) or (readByte().toLong() and 0xFFL) }
    return result
}

internal fun ByteReader.readFloat(): Float = Float.fromBits(readInt())

internal fun ByteReader.readDouble(): Double = Double.fromBits(readLong())

internal class GrowableByteWriter(
    initialCapacity: Int = 64,
) : ByteWriter {
    private var buffer = ByteArray(initialCapacity)
    private var size = 0

    override fun writeByte(value: Byte) {
        ensureCapacity(size + 1)
        buffer[size++] = value
    }

    fun toByteArray(): ByteArray = buffer.copyOf(size)

    private fun ensureCapacity(minCapacity: Int) {
        if (minCapacity > buffer.size) {
            buffer = buffer.copyOf(maxOf(minCapacity, buffer.size * 2))
        }
    }
}

internal class ArrayByteReader(
    private val array: ByteArray,
) : ByteReader {
    var position: Int = 0
        private set

    val hasRemaining: Boolean get() = position < array.size

    override fun readByte(): Byte {
        if (position >= array.size) throw NbtFormatException("Unexpected end of NBT data")
        return array[position++]
    }
}
