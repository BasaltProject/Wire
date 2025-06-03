package org.basaltmc.protocol

import org.basaltmc.io.ByteBuffer
import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertFailsWith

class VarIntTest {
    private fun roundTrip(value: Int): Int {
        val buffer = ByteBuffer()
        VarInt.write(buffer, value)
        return VarInt.read(buffer)
    }

    @Test
    fun roundTripsRepresentativeValues() {
        for (value in listOf(0, 1, -1, 127, 128, 255, 25565, 2097151, Int.MAX_VALUE, Int.MIN_VALUE, -25565)) {
            assertEquals(value, roundTrip(value))
        }
    }

    @Test
    fun matchesWellKnownEncodedBytes() {
        // Widely documented reference encodings for the Minecraft protocol VarInt.
        fun bytesOf(value: Int): List<Byte> {
            val buffer = ByteBuffer()
            VarInt.write(buffer, value)
            return buffer.toByteArray().toList()
        }
        assertEquals(listOf(0x00), bytesOf(0).map { it.toInt() and 0xFF })
        assertEquals(listOf(0x01), bytesOf(1).map { it.toInt() and 0xFF })
        assertEquals(listOf(0xDD, 0xC7, 0x01), bytesOf(25565).map { it.toInt() and 0xFF })
        assertEquals(listOf(0xFF, 0xFF, 0xFF, 0xFF, 0x07), bytesOf(Int.MAX_VALUE).map { it.toInt() and 0xFF })
        assertEquals(listOf(0xFF, 0xFF, 0xFF, 0xFF, 0x0F), bytesOf(-1).map { it.toInt() and 0xFF })
    }

    @Test
    fun sizeOfMatchesActualEncodedLength() {
        for (value in listOf(0, 1, 127, 128, 16383, 16384, 2097151, 2097152, Int.MAX_VALUE, Int.MIN_VALUE, -1)) {
            val buffer = ByteBuffer()
            VarInt.write(buffer, value)
            assertEquals(buffer.readableBytes, VarInt.sizeOf(value), "size mismatch for $value")
        }
    }

    @Test
    fun rejectsOverlongEncoding() {
        val buffer = ByteBuffer()
        repeat(6) { buffer.writeByte(0xFF.toByte()) }
        assertFailsWith<ProtocolException> { VarInt.read(buffer) }
    }
}
