package org.basaltmc.wire.io

import kotlin.test.Test
import kotlin.test.assertEquals

class RawInflateTest {
    @Test
    fun decompressesARealRawDeflateStream() {
        // Generated with Python's zlib.compressobj(9, zlib.DEFLATED, -15) (wbits=-15: raw
        // deflate, matching ZIP's "deflate" storage method, no zlib/gzip wrapper).
        val compressed =
            byteArrayOf(
                -13,
                72,
                -51,
                -55,
                -55,
                -41,
                81,
                112,
                74,
                44,
                78,
                -52,
                41,
                81,
                -120,
                -14,
                12,
                80,
                40,
                73,
                45,
                46,
                81,
                84,
                -16,
                32,
                81,
                28,
                0,
            )
        val expected = "Hello, Basalt ZIP test! ".repeat(3)
        val result = RawInflate.decompress(compressed, uncompressedSize = expected.length)
        assertEquals(expected, result.decodeToString())
    }

    @Test
    fun decompressesEmptyInputToEmptyOutput() {
        assertEquals(0, RawInflate.decompress(byteArrayOf(1), uncompressedSize = 0).size)
    }
}
