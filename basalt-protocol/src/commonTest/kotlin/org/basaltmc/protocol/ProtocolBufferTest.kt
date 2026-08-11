package org.basaltmc.protocol

import org.basaltmc.io.ByteBuffer
import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertFailsWith
import kotlin.uuid.ExperimentalUuidApi
import kotlin.uuid.Uuid

class ProtocolBufferTest {
    @Test
    fun roundTripsStrings() {
        val buffer = ByteBuffer()
        buffer.writeProtocolString("hello world")
        assertEquals("hello world", buffer.readProtocolString())
    }

    @Test
    fun roundTripsNonAsciiStrings() {
        val buffer = ByteBuffer()
        val text = "héllo wörld 你好 😀"
        buffer.writeProtocolString(text)
        assertEquals(text, buffer.readProtocolString())
    }

    @Test
    fun rejectsStringsLongerThanMax() {
        val buffer = ByteBuffer()
        assertFailsWith<IllegalArgumentException> { buffer.writeProtocolString("hello", maxLength = 3) }
    }

    @OptIn(ExperimentalUuidApi::class)
    @Test
    fun roundTripsUuids() {
        val buffer = ByteBuffer()
        val uuid = Uuid.parse("f47ac10b-58cc-4372-a567-0e02b2c3d479")
        buffer.writeUuid(uuid)
        assertEquals(uuid, buffer.readUuid())
    }

    @Test
    fun roundTripsBooleans() {
        val buffer = ByteBuffer()
        buffer.writeBoolean(true)
        buffer.writeBoolean(false)
        assertEquals(true, buffer.readBoolean())
        assertEquals(false, buffer.readBoolean())
    }
}
