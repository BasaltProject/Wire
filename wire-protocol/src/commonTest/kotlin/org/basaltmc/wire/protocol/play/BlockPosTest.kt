package org.basaltmc.wire.protocol.play

import kotlin.test.Test
import kotlin.test.assertEquals

class BlockPosTest {
    @Test
    fun roundTripsPositiveCoordinates() {
        val pos = BlockPos(100, 64, 200)
        assertEquals(pos, BlockPos.unpack(pos.pack()))
    }

    @Test
    fun roundTripsNegativeCoordinates() {
        // -30000000 still fits the 26-bit signed x/z range (+-33554432).
        val pos = BlockPos(-1, -64, -30000000)
        assertEquals(pos, BlockPos.unpack(pos.pack()))
    }

    @Test
    fun roundTripsOrigin() {
        assertEquals(BlockPos(0, 0, 0), BlockPos.unpack(BlockPos(0, 0, 0).pack()))
    }

    @Test
    fun roundTripsExtremeYWithinRange() {
        val pos = BlockPos(0, -2048, 0)
        assertEquals(pos, BlockPos.unpack(pos.pack()))
        val pos2 = BlockPos(0, 2047, 0)
        assertEquals(pos2, BlockPos.unpack(pos2.pack()))
    }

    @Test
    fun roundTripsRealisticFlatWorldSpawn() {
        val pos = BlockPos(0, 4, 0)
        assertEquals(pos, BlockPos.unpack(pos.pack()))
    }
}
