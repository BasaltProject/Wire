package org.basaltmc.wire.protocol.play

import org.basaltmc.wire.io.ByteBuffer
import org.basaltmc.wire.protocol.readVarInt
import org.basaltmc.wire.protocol.writeVarInt

/**
 * The block/biome section codec vanilla's `net.minecraft.world.level.chunk.PalettedContainer`
 * uses inside chunk data: single-valued (0 bits, one palette entry, no data longs), indirect
 * (a small local palette plus [BitStorage]-packed indices into it), or direct (no palette,
 * [BitStorage] packed with the real registry IDs themselves). Which one applies is decided from
 * the section's distinct value count, not chosen by the caller.
 */
public object PalettedContainer {
    public const val BLOCK_MIN_INDIRECT_BITS: Int = 4
    public const val BLOCK_MAX_INDIRECT_BITS: Int = 8
    public const val BIOME_MIN_INDIRECT_BITS: Int = 0
    public const val BIOME_MAX_INDIRECT_BITS: Int = 3

    public fun write(
        buffer: ByteBuffer,
        values: IntArray,
        minIndirectBits: Int,
        maxIndirectBits: Int,
        directBits: Int,
    ) {
        val palette = LinkedHashSet<Int>()
        for (value in values) palette.add(value)

        if (palette.size == 1) {
            buffer.writeByte(0.toByte())
            buffer.writeVarInt(palette.first())
            // No data array follows: vanilla's zero-bit storage has zero longs, and (like the
            // indirect/direct cases below) the length is never written on the wire, only computed
            // from bitsPerEntry and the container size (`FriendlyByteBuf.writeFixedSizeLongArray`).
            return
        }

        val neededBits = bitsFor(palette.size)
        if (neededBits <= maxIndirectBits) {
            val bitsPerEntry = neededBits.coerceAtLeast(minIndirectBits)
            buffer.writeByte(bitsPerEntry.toByte())
            val paletteList = palette.toList()
            buffer.writeVarInt(paletteList.size)
            for (value in paletteList) buffer.writeVarInt(value)
            val indexOf = paletteList.withIndex().associate { (index, value) -> value to index }
            val storage = BitStorage(bitsPerEntry, values.size)
            for (i in values.indices) storage[i] = indexOf.getValue(values[i])
            for (long in storage.data) buffer.writeLong(long)
        } else {
            buffer.writeByte(directBits.toByte())
            val storage = BitStorage(directBits, values.size)
            for (i in values.indices) storage[i] = values[i]
            for (long in storage.data) buffer.writeLong(long)
        }
    }

    public fun read(
        buffer: ByteBuffer,
        size: Int,
        maxIndirectBits: Int,
    ): IntArray {
        val bitsPerEntry = buffer.readByte().toInt() and 0xFF
        if (bitsPerEntry == 0) {
            val value = buffer.readVarInt()
            // No data array to read: matches write's zero-bit case (see its comment).
            return IntArray(size) { value }
        }

        val palette =
            if (bitsPerEntry <= maxIndirectBits) {
                IntArray(buffer.readVarInt()) { buffer.readVarInt() }
            } else {
                null
            }
        // The data array's length is never sent on the wire (vanilla's `readFixedSizeLongArray`):
        // it's derived from bitsPerEntry and the container's own known size, same formula as
        // BitStorage's own longCount.
        val valuesPerLong = 64 / bitsPerEntry
        val longCount = (size + valuesPerLong - 1) / valuesPerLong
        val data = LongArray(longCount) { buffer.readLong() }
        val storage = BitStorage(bitsPerEntry, size, data)
        return IntArray(size) { i -> palette?.get(storage[i]) ?: storage[i] }
    }

    private fun bitsFor(distinctCount: Int): Int {
        var bits = 0
        while ((1 shl bits) < distinctCount) bits++
        return bits
    }
}
