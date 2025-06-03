package org.basaltmc.wire.protocol.play

/**
 * Fixed-width values packed into `i64`s, matching vanilla's `SimpleBitStorage`: entries never
 * span two longs (the remaining high bits of each long are left unused padding), unlike the old
 * pre-1.16 scheme that packed across long boundaries.
 */
public class BitStorage(
    public val bitsPerEntry: Int,
    public val size: Int,
    data: LongArray? = null,
) {
    private val valuesPerLong: Int = if (bitsPerEntry == 0) 0 else 64 / bitsPerEntry
    private val mask: Long = if (bitsPerEntry == 0) 0L else (1L shl bitsPerEntry) - 1L
    private val longCount: Int = if (bitsPerEntry == 0) 0 else (size + valuesPerLong - 1) / valuesPerLong

    public val data: LongArray = data ?: LongArray(longCount)

    init {
        require(bitsPerEntry in 0..32) { "bitsPerEntry out of range: $bitsPerEntry" }
        require(this.data.size == longCount) { "expected $longCount longs, got ${this.data.size}" }
    }

    public operator fun get(index: Int): Int {
        if (bitsPerEntry == 0) return 0
        require(index in 0 until size) { "index out of bounds: $index" }
        val longIndex = index / valuesPerLong
        val bitOffset = (index % valuesPerLong) * bitsPerEntry
        return ((data[longIndex] ushr bitOffset) and mask).toInt()
    }

    public operator fun set(
        index: Int,
        value: Int,
    ) {
        require(bitsPerEntry > 0) { "cannot store a value in a zero-bit storage" }
        require(index in 0 until size) { "index out of bounds: $index" }
        require(value.toLong() and mask.inv() == 0L) { "value $value does not fit in $bitsPerEntry bits" }
        val longIndex = index / valuesPerLong
        val bitOffset = (index % valuesPerLong) * bitsPerEntry
        data[longIndex] = (data[longIndex] and (mask shl bitOffset).inv()) or (value.toLong() shl bitOffset)
    }
}
