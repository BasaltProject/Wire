package org.basaltmc.io

/**
 * A growable, big-endian byte buffer with independent read/write cursors, in the spirit of
 * Netty's `ByteBuf`: writes append at [writerIndex] and grow the backing array as needed, reads
 * consume from [readerIndex]. This is the buffer type `basalt-protocol`'s packet codecs are
 * built on; it has no network or platform dependency of its own.
 */
public class ByteBuffer(
    initialCapacity: Int = 256,
) {
    private var array: ByteArray = ByteArray(initialCapacity)

    public var readerIndex: Int = 0
        private set

    public var writerIndex: Int = 0
        private set

    public val readableBytes: Int get() = writerIndex - readerIndex
    public val writableBytes: Int get() = array.size - writerIndex
    public val capacity: Int get() = array.size

    public fun ensureWritable(minBytes: Int) {
        val required = writerIndex + minBytes
        if (required <= array.size) return
        var newCapacity = if (array.size == 0) 64 else array.size
        while (newCapacity < required) newCapacity *= 2
        array = array.copyOf(newCapacity)
    }

    public fun writeByte(value: Byte) {
        ensureWritable(1)
        array[writerIndex++] = value
    }

    public fun writeBytes(
        src: ByteArray,
        offset: Int = 0,
        length: Int = src.size - offset,
    ) {
        ensureWritable(length)
        src.copyInto(array, writerIndex, offset, offset + length)
        writerIndex += length
    }

    public fun writeShort(value: Short) {
        val v = value.toInt()
        writeByte((v shr 8).toByte())
        writeByte(v.toByte())
    }

    public fun writeInt(value: Int) {
        writeByte((value shr 24).toByte())
        writeByte((value shr 16).toByte())
        writeByte((value shr 8).toByte())
        writeByte(value.toByte())
    }

    public fun writeLong(value: Long) {
        for (shift in 56 downTo 0 step 8) writeByte((value ushr shift).toByte())
    }

    public fun writeFloat(value: Float): Unit = writeInt(value.toRawBits())

    public fun writeDouble(value: Double): Unit = writeLong(value.toRawBits())

    public fun readByte(): Byte {
        checkReadable(1)
        return array[readerIndex++]
    }

    public fun readBytes(length: Int): ByteArray {
        checkReadable(length)
        val result = array.copyOfRange(readerIndex, readerIndex + length)
        readerIndex += length
        return result
    }

    public fun readShort(): Short {
        val a = readByte().toInt() and 0xFF
        val b = readByte().toInt() and 0xFF
        return ((a shl 8) or b).toShort()
    }

    public fun readInt(): Int {
        var result = 0
        repeat(4) { result = (result shl 8) or (readByte().toInt() and 0xFF) }
        return result
    }

    public fun readLong(): Long {
        var result = 0L
        repeat(8) { result = (result shl 8) or (readByte().toLong() and 0xFFL) }
        return result
    }

    public fun readFloat(): Float = Float.fromBits(readInt())

    public fun readDouble(): Double = Double.fromBits(readLong())

    public fun peekByte(offset: Int = 0): Byte {
        checkReadable(offset + 1)
        return array[readerIndex + offset]
    }

    public fun skip(length: Int) {
        checkReadable(length)
        readerIndex += length
    }

    /** Moves any unread bytes to the start of the backing array, resetting both cursors. */
    public fun discardReadBytes() {
        if (readerIndex == 0) return
        val remaining = readableBytes
        array.copyInto(array, 0, readerIndex, writerIndex)
        readerIndex = 0
        writerIndex = remaining
    }

    public fun clear() {
        readerIndex = 0
        writerIndex = 0
    }

    /** A copy of the unread region, from [readerIndex] to [writerIndex]. Does not consume. */
    public fun toByteArray(): ByteArray = array.copyOfRange(readerIndex, writerIndex)

    private fun checkReadable(length: Int) {
        if (length > readableBytes) {
            throw IndexOutOfBoundsException(
                "Cannot read $length bytes: only $readableBytes readable (reader=$readerIndex, writer=$writerIndex)",
            )
        }
    }
}
