package org.basaltmc.wire.protocol.play

import org.basaltmc.wire.io.ByteBuffer
import kotlin.test.Test
import kotlin.test.assertEquals

class PalettedContainerTest {
    private fun roundTripBlocks(values: IntArray): IntArray {
        val buffer = ByteBuffer()
        PalettedContainer.write(
            buffer,
            values,
            PalettedContainer.BLOCK_MIN_INDIRECT_BITS,
            PalettedContainer.BLOCK_MAX_INDIRECT_BITS,
            directBits = 8,
        )
        return PalettedContainer.read(buffer, values.size, PalettedContainer.BLOCK_MAX_INDIRECT_BITS)
    }

    private fun roundTripBiomes(values: IntArray): IntArray {
        val buffer = ByteBuffer()
        PalettedContainer.write(
            buffer,
            values,
            PalettedContainer.BIOME_MIN_INDIRECT_BITS,
            PalettedContainer.BIOME_MAX_INDIRECT_BITS,
            directBits = 6,
        )
        return PalettedContainer.read(buffer, values.size, PalettedContainer.BIOME_MAX_INDIRECT_BITS)
    }

    @Test
    fun singleValuedSectionRoundTrips() {
        val values = IntArray(4096) { 0 }
        assertEquals(values.toList(), roundTripBlocks(values).toList())
    }

    @Test
    fun indirectPaletteRoundTripsBlockValues() {
        val values = IntArray(4096) { i -> intArrayOf(1, 10, 10, 9)[i % 4] }
        assertEquals(values.toList(), roundTripBlocks(values).toList())
    }

    @Test
    fun forcesAtLeastFourBitsForBlocksEvenWithTwoDistinctValues() {
        val buffer = ByteBuffer()
        val values = IntArray(4096) { i -> if (i % 2 == 0) 0 else 1 }
        PalettedContainer.write(
            buffer,
            values,
            PalettedContainer.BLOCK_MIN_INDIRECT_BITS,
            PalettedContainer.BLOCK_MAX_INDIRECT_BITS,
            8,
        )
        assertEquals(4, buffer.toByteArray()[0].toInt())
    }

    @Test
    fun biomesUseNoForcedMinimumBits() {
        val buffer = ByteBuffer()
        val values = IntArray(64) { i -> i % 2 }
        PalettedContainer.write(
            buffer,
            values,
            PalettedContainer.BIOME_MIN_INDIRECT_BITS,
            PalettedContainer.BIOME_MAX_INDIRECT_BITS,
            6,
        )
        assertEquals(1, buffer.toByteArray()[0].toInt())
    }

    @Test
    fun indirectPaletteRoundTripsBiomeValues() {
        val values = IntArray(64) { i -> i % 3 }
        assertEquals(values.toList(), roundTripBiomes(values).toList())
    }

    @Test
    fun directModeRoundTripsWhenDistinctCountExceedsIndirectRange() {
        // 300 distinct values need 9 bits, past BLOCK_MAX_INDIRECT_BITS (8): falls back to direct.
        val values = IntArray(4096) { i -> i % 300 }
        val buffer = ByteBuffer()
        PalettedContainer.write(
            buffer,
            values,
            PalettedContainer.BLOCK_MIN_INDIRECT_BITS,
            PalettedContainer.BLOCK_MAX_INDIRECT_BITS,
            directBits = 9,
        )
        val result = PalettedContainer.read(buffer, values.size, PalettedContainer.BLOCK_MAX_INDIRECT_BITS)
        assertEquals(values.toList(), result.toList())
    }
}
