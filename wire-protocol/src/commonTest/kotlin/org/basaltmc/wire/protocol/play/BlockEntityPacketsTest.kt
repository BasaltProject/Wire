package org.basaltmc.wire.protocol.play

import org.basaltmc.wire.io.ByteBuffer
import org.basaltmc.wire.nbt.NbtCompound
import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertFailsWith
import kotlin.test.assertNull

class BlockEntityPacketsTest {
    @Test
    fun blockEntityDataRoundTrips() {
        val tag = NbtCompound().apply { putString("color", "black") }
        val buffer = ByteBuffer()
        BlockEntityDataPacket(BlockPos(10, -60, 300), typeId = 7, tag = tag).write(buffer)

        val read = BlockEntityDataPacket.read(buffer)
        assertEquals(BlockPos(10, -60, 300), read.pos)
        assertEquals(7, read.typeId)
        assertEquals("black", read.tag?.getString("color"))
    }

    /** An empty tag is how vanilla says "nothing here", and it has to survive the round trip. */
    @Test
    fun blockEntityDataCarriesNoTagAtAll() {
        val buffer = ByteBuffer()
        BlockEntityDataPacket(BlockPos(0, 0, 0), typeId = 1, tag = null).write(buffer)

        assertNull(BlockEntityDataPacket.read(buffer).tag)
    }

    @Test
    fun openSignEditorRoundTrips() {
        val buffer = ByteBuffer()
        OpenSignEditorPacket(BlockPos(-4, 70, 12), isFrontText = false).write(buffer)

        val read = OpenSignEditorPacket.read(buffer)
        assertEquals(BlockPos(-4, 70, 12), read.pos)
        assertEquals(false, read.isFrontText)
    }

    @Test
    fun signUpdateRoundTripsItsFourLines() {
        val lines = listOf("first", "", "third with spaces", "cuarta")
        val buffer = ByteBuffer()
        SignUpdatePacket(BlockPos(1, 2, 3), isFrontText = true, lines = lines).write(buffer)

        val read = SignUpdatePacket.read(buffer)
        assertEquals(BlockPos(1, 2, 3), read.pos)
        assertEquals(true, read.isFrontText)
        assertEquals(lines, read.lines)
    }

    @Test
    fun aSignIsAlwaysFourLines() {
        assertFailsWith<IllegalArgumentException> {
            SignUpdatePacket(BlockPos(0, 0, 0), isFrontText = true, lines = listOf("one", "two"))
        }
    }
}
