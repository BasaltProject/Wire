package org.basaltmc.wire.io

import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertTrue

class GzipTest {
    @Test
    fun decompressesARealGzipStream() {
        // Generated with Python's gzip.compress(b"hello basalt", mtime=0).
        val compressed =
            byteArrayOf(
                31,
                -117,
                8,
                0,
                0,
                0,
                0,
                0,
                2,
                -1,
                -53,
                72,
                -51,
                -55,
                -55,
                87,
                72,
                74,
                44,
                78,
                -52,
                41,
                1,
                0,
                97,
                52,
                35,
                -116,
                12,
                0,
                0,
                0,
            )
        val decompressed = Gzip.decompress(compressed)
        assertEquals("hello basalt", decompressed.decodeToString())
    }

    @Test
    fun rejectsInputTooSmallToBeGzip() {
        var threw = false
        try {
            Gzip.decompress(byteArrayOf(1, 2, 3))
        } catch (_: IllegalArgumentException) {
            threw = true
        }
        assertTrue(threw)
    }
}
