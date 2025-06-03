package org.basaltmc.io

import kotlin.random.Random
import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertFailsWith
import kotlin.test.assertTrue

class ZlibTest {
    @Test
    fun compressThenDecompressRoundTripsWithKnownSize() {
        val original = "The quick brown fox jumps over the lazy dog".repeat(20).encodeToByteArray()
        val compressed = Zlib.compress(original)
        assertTrue(compressed.size < original.size, "compressed output should be smaller for repetitive input")
        val decompressed = Zlib.decompress(compressed, original.size)
        assertEquals(original.toList(), decompressed.toList())
    }

    @Test
    fun roundTripsRandomBinaryData() {
        val random = Random(42)
        val original = ByteArray(10_000) { random.nextInt(256).toByte() }
        val compressed = Zlib.compress(original)
        val decompressed = Zlib.decompress(compressed, original.size)
        assertEquals(original.toList(), decompressed.toList())
    }

    @Test
    fun decompressUnknownSizeGrowsUntilItFits() {
        val original = ByteArray(200_000) { (it % 251).toByte() }
        val compressed = Zlib.compress(original)
        val decompressed = Zlib.decompressUnknownSize(compressed, initialCapacity = 16)
        assertEquals(original.toList(), decompressed.toList())
    }

    @Test
    fun emptyInputRoundTrips() {
        assertEquals(0, Zlib.compress(ByteArray(0)).size)
        assertEquals(0, Zlib.decompress(ByteArray(0), 0).size)
    }

    @Test
    fun corruptDataThrowsZlibException() {
        val garbage = byteArrayOf(1, 2, 3, 4, 5, 6, 7, 8)
        assertFailsWith<ZlibException> { Zlib.decompress(garbage, 100) }
    }
}
