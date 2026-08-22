package org.basaltmc.wire.protocol.play

import org.basaltmc.wire.io.ByteBuffer
import kotlin.test.Test
import kotlin.test.assertContentEquals
import kotlin.test.assertEquals

class CustomPayloadPacketTest {
    private fun roundTrip(packet: CustomPayloadPacket): CustomPayloadPacket {
        val buffer = ByteBuffer()
        packet.write(buffer)
        return CustomPayloadPacket.read(buffer)
    }

    @Test
    fun aPayloadSurvivesTheRoundTrip() {
        val packet = CustomPayloadPacket("arena:main", byteArrayOf(1, 2, 3, 0, -128, 127))
        assertEquals(packet, roundTrip(packet))
    }

    @Test
    fun anEmptyPayloadIsAMessageToo() {
        val packet = CustomPayloadPacket("arena:ping", ByteArray(0))
        val back = roundTrip(packet)
        assertEquals("arena:ping", back.channel)
        assertContentEquals(ByteArray(0), back.data)
    }

    @Test
    fun theBodyIsWhateverIsLeftRatherThanACountedRun() {
        // No length prefix: a reader that stopped early would leave the frame half consumed, which
        // is the shape of the bug this is checking for.
        val buffer = ByteBuffer()
        CustomPayloadPacket("a:b", byteArrayOf(9, 9, 9)).write(buffer)
        val back = CustomPayloadPacket.read(buffer)
        assertEquals(0, buffer.readableBytes)
        assertContentEquals(byteArrayOf(9, 9, 9), back.data)
    }
}
