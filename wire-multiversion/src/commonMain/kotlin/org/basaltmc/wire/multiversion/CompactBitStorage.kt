package org.basaltmc.wire.multiversion

/**
 * The way values were packed into longs before 1.16: end to end, spilling across the boundary.
 *
 * 1.16 stopped an entry ever spanning two longs and left the top of each long unused instead, so
 * every version below it needs the tight packing written back, and a link below that one has to
 * read what the link above it wrote.
 */
public object CompactBitStorage {
    public fun longsFor(
        size: Int,
        bits: Int,
    ): Int = (size * bits + Long.SIZE_BITS - 1) / Long.SIZE_BITS

    /** The [size] entries of [longs], each [bits] wide, packed the old way. */
    public fun unpack(
        longs: LongArray,
        bits: Int,
        size: Int,
    ): IntArray {
        val values = IntArray(size)
        val mask = (1L shl bits) - 1L
        for (index in 0 until size) {
            val bit = index * bits
            val slot = bit / Long.SIZE_BITS
            val offset = bit % Long.SIZE_BITS
            var value = (longs[slot] ushr offset) and mask
            val spilled = offset + bits - Long.SIZE_BITS
            if (spilled > 0) value = value or ((longs[slot + 1] and ((1L shl spilled) - 1L)) shl (bits - spilled))
            values[index] = value.toInt()
        }
        return values
    }

    /** [values] as [bits]-wide entries, packed the old way. */
    public fun pack(
        values: IntArray,
        bits: Int,
    ): LongArray {
        val longs = LongArray(longsFor(values.size, bits))
        val mask = (1L shl bits) - 1L
        for (index in values.indices) {
            val bit = index * bits
            val slot = bit / Long.SIZE_BITS
            val offset = bit % Long.SIZE_BITS
            val value = values[index].toLong() and mask
            longs[slot] = longs[slot] or (value shl offset)
            val spilled = offset + bits - Long.SIZE_BITS
            if (spilled > 0) longs[slot + 1] = longs[slot + 1] or (value ushr (bits - spilled))
        }
        return longs
    }
}
