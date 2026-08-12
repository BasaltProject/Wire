package org.basaltmc.wire.io

import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertFailsWith
import kotlin.test.assertTrue

class ByteBufferTest {
    @Test
    fun writeThenReadRoundTripsAllPrimitives() {
        val buffer = ByteBuffer(initialCapacity = 4) // force growth
        buffer.writeByte(-12)
        buffer.writeShort(1234)
        buffer.writeInt(-70000)
        buffer.writeLong(1234567890123L)
        buffer.writeFloat(3.14f)
        buffer.writeDouble(2.718281828459045)
        buffer.writeBytes(byteArrayOf(1, 2, 3, 4))

        assertEquals((-12).toByte(), buffer.readByte())
        assertEquals(1234.toShort(), buffer.readShort())
        assertEquals(-70000, buffer.readInt())
        assertEquals(1234567890123L, buffer.readLong())
        assertEquals(3.14f, buffer.readFloat())
        assertEquals(2.718281828459045, buffer.readDouble())
        assertEquals(listOf<Byte>(1, 2, 3, 4), buffer.readBytes(4).toList())
        assertEquals(0, buffer.readableBytes)
    }

    @Test
    fun growsCapacityAutomatically() {
        val buffer = ByteBuffer(initialCapacity = 2)
        repeat(1000) { buffer.writeByte(it.toByte()) }
        assertEquals(1000, buffer.readableBytes)
        assertTrue(buffer.capacity >= 1000)
        repeat(1000) { i -> assertEquals(i.toByte(), buffer.readByte()) }
    }

    @Test
    fun peekDoesNotConsume() {
        val buffer = ByteBuffer()
        buffer.writeByte(42)
        assertEquals(42.toByte(), buffer.peekByte())
        assertEquals(1, buffer.readableBytes)
        assertEquals(42.toByte(), buffer.readByte())
        assertEquals(0, buffer.readableBytes)
    }

    @Test
    fun discardReadBytesCompactsBuffer() {
        val buffer = ByteBuffer()
        buffer.writeBytes(byteArrayOf(1, 2, 3, 4, 5))
        buffer.readBytes(3)
        buffer.discardReadBytes()
        assertEquals(0, buffer.readerIndex)
        assertEquals(2, buffer.writerIndex)
        assertEquals(listOf<Byte>(4, 5), buffer.toByteArray().toList())
    }

    @Test
    fun readingPastWriterIndexThrows() {
        val buffer = ByteBuffer()
        buffer.writeByte(1)
        buffer.readByte()
        assertFailsWith<IndexOutOfBoundsException> { buffer.readByte() }
    }

    @Test
    fun clearResetsBothCursors() {
        val buffer = ByteBuffer()
        buffer.writeBytes(byteArrayOf(1, 2, 3))
        buffer.readByte()
        buffer.clear()
        assertEquals(0, buffer.readerIndex)
        assertEquals(0, buffer.writerIndex)
    }
}
