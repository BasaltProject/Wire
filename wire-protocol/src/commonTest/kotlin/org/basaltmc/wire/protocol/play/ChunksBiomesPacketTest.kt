package org.basaltmc.wire.protocol.play

import org.basaltmc.wire.io.ByteBuffer
import kotlin.test.Test
import kotlin.test.assertEquals

class ChunksBiomesPacketTest {
    @Test
    fun roundTripsEveryColumn() {
        val packet =
            ChunksBiomesPacket(
                listOf(
                    ChunkBiomeData(0, 0, byteArrayOf(0, 1, 2)),
                    ChunkBiomeData(-3, 7, byteArrayOf(9)),
                ),
            )
        val buffer = ByteBuffer()
        packet.write(buffer)
        assertEquals(packet, ChunksBiomesPacket.read(ByteBuffer.wrap(buffer.toByteArray())))
    }

    @Test
    fun negativeCoordinatesSurviveThePackedLong() {
        val packed = ChunkBiomeData.packChunkPos(-1, -2)
        val buffer = ByteBuffer()
        ChunkBiomeData(-1, -2, byteArrayOf()).write(buffer)
        val read = ChunkBiomeData.read(ByteBuffer.wrap(buffer.toByteArray()))
        assertEquals(-1, read.chunkX)
        assertEquals(-2, read.chunkZ)
        // parity: the same packing `ChunkPos.asLong` uses, so a client reads the pair back the same way.
        assertEquals(packed, (-1L and 0xFFFFFFFFL) or ((-2L and 0xFFFFFFFFL) shl 32))
    }
}
