package org.basaltmc.io

import kotlin.random.Random
import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertFailsWith
import kotlin.test.assertTrue

class ZstdTest {
    @Test
    fun compressThenDecompressRoundTripsWithKnownSize() {
        val original = "The quick brown fox jumps over the lazy dog".repeat(20).encodeToByteArray()
        val compressed = Zstd.compress(original)
        assertTrue(compressed.size < original.size, "compressed output should be smaller for repetitive input")
        val decompressed = Zstd.decompress(compressed)
        assertEquals(original.toList(), decompressed.toList())
    }

    @Test
    fun roundTripsRandomBinaryData() {
        val random = Random(42)
        val original = ByteArray(10_000) { random.nextInt(256).toByte() }
        val compressed = Zstd.compress(original)
        val decompressed = Zstd.decompress(compressed)
        assertEquals(original.toList(), decompressed.toList())
    }

    @Test
    fun emptyInputRoundTrips() {
        assertEquals(0, Zstd.compress(ByteArray(0)).size)
        assertEquals(0, Zstd.decompress(ByteArray(0)).size)
    }

    @Test
    fun corruptDataThrowsZstdException() {
        val garbage = byteArrayOf(1, 2, 3, 4, 5, 6, 7, 8)
        assertFailsWith<ZstdException> { Zstd.decompress(garbage) }
    }

    @Test
    fun roundTripsInputLargerThanTheInternalChunkSize() {
        // Bigger than the 64 KiB chunk size Zstd.compress/decompress loop over internally, to
        // exercise the multi-iteration path, not just a single compressStream2/decompressStream call.
        val original = ByteArray(500_000) { (it % 251).toByte() }
        val compressed = Zstd.compress(original)
        val decompressed = Zstd.decompress(compressed)
        assertEquals(original.toList(), decompressed.toList())
    }

    @Test
    fun higherCompressionLevelsStillRoundTrip() {
        val original = ByteArray(50_000) { (it % 17).toByte() }
        val compressed = Zstd.compress(original, level = 19)
        val decompressed = Zstd.decompress(compressed)
        assertEquals(original.toList(), decompressed.toList())
    }
}
