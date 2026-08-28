package org.basaltmc.wire.protocol.login

import org.basaltmc.wire.io.ByteBuffer
import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertNull

class LoginPluginPacketsTest {
    @Test
    fun requestRoundTrips() {
        val packet = LoginPluginRequestPacket(0x1234, "velocity:player_info", byteArrayOf(4))
        val buffer = ByteBuffer()
        packet.write(buffer)

        assertEquals(packet, LoginPluginRequestPacket.read(buffer))
    }

    @Test
    fun responseRoundTrips() {
        val answered = LoginPluginResponsePacket(0x1234, ByteArray(40) { it.toByte() })
        val buffer = ByteBuffer()
        answered.write(buffer)
        assertEquals(answered, LoginPluginResponsePacket.read(buffer))

        // What a client that does not know the channel sends, which is every real one.
        val unanswered = LoginPluginResponsePacket(0x1234, null)
        val second = ByteBuffer()
        unanswered.write(second)
        val read = LoginPluginResponsePacket.read(second)
        assertEquals(unanswered, read)
        assertNull(read.payload)
    }
}
