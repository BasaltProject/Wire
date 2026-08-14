package org.basaltmc.wire.io

import kotlin.random.Random
import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertFailsWith
import kotlin.test.assertFalse
import kotlin.test.assertTrue

class ZstdStreamReaderTest {
    @Test
    fun readsAFrameInOddSizedPiecesWithoutAWholeCopy() {
        val original = ByteArray(300_000).also { Random(7).nextBytes(it) }
        val compressed = Zstd.compress(original)

        val reassembled = ByteArray(original.size)
        ZstdStreamReader(compressed).use { reader ->
            var offset = 0
            // Deliberately odd piece sizes, crossing every internal boundary alignment.
            for (piece in intArrayOf(1, 8191, 65_536, 100_003)) {
                reader.readFully(reassembled, offset, piece)
                offset += piece
            }
            reader.readFully(reassembled, offset, original.size - offset)
            assertTrue(reader.frameComplete, "frame should be complete after the declared size")
        }
        assertEquals(original.toList(), reassembled.toList())
    }

    @Test
    fun windowedInputReadsOnlyItsRange() {
        val original = "windowed".repeat(5_000).encodeToByteArray()
        val compressed = Zstd.compress(original)
        val padded = ByteArray(compressed.size + 64)
        compressed.copyInto(padded, 32)

        val out = ByteArray(original.size)
        ZstdStreamReader(padded, inputFrom = 32, inputTo = 32 + compressed.size).use { reader ->
            reader.readFully(out)
            assertTrue(reader.frameComplete)
        }
        assertEquals(original.toList(), out.toList())
    }

    @Test
    fun readingPastTheFrameThrows() {
        val compressed = Zstd.compress(ByteArray(1000))
        ZstdStreamReader(compressed).use { reader ->
            reader.readFully(ByteArray(1000))
            assertFailsWith<ZstdException> { reader.readFully(ByteArray(1)) }
        }
    }

    @Test
    fun aShortReadLeavesTheFrameIncomplete() {
        val compressed = Zstd.compress(ByteArray(50_000).also { Random(3).nextBytes(it) })
        ZstdStreamReader(compressed).use { reader ->
            reader.readFully(ByteArray(10))
            assertFalse(reader.frameComplete)
        }
    }
}
