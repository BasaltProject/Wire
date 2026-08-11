package org.basaltmc.protocol.play

/** Packs 4096 4-bit light values (one per block in a section) into 2048 bytes, low nibble first. */
public object NibbleArray {
    public const val SECTION_BYTE_COUNT: Int = 4096 / 2

    public fun pack(values: IntArray): ByteArray {
        require(values.size == 4096) { "expected 4096 values, got ${values.size}" }
        val bytes = ByteArray(SECTION_BYTE_COUNT)
        for (i in 0 until SECTION_BYTE_COUNT) {
            val low = values[i * 2] and 0xF
            val high = values[i * 2 + 1] and 0xF
            bytes[i] = ((high shl 4) or low).toByte()
        }
        return bytes
    }

    public fun unpack(bytes: ByteArray): IntArray {
        require(bytes.size == SECTION_BYTE_COUNT) { "expected $SECTION_BYTE_COUNT bytes, got ${bytes.size}" }
        val values = IntArray(4096)
        for (i in 0 until SECTION_BYTE_COUNT) {
            val byte = bytes[i].toInt() and 0xFF
            values[i * 2] = byte and 0xF
            values[i * 2 + 1] = (byte shr 4) and 0xF
        }
        return values
    }

    /** A section filled with a single light level everywhere, the only case Basalt's flat-world lighting needs. */
    public fun uniform(level: Int): ByteArray {
        require(level in 0..15) { "light level out of range: $level" }
        val byte = ((level shl 4) or level).toByte()
        return ByteArray(SECTION_BYTE_COUNT) { byte }
    }
}
