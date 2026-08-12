package org.basaltmc.wire.protocol.handshake

import org.basaltmc.wire.io.ByteBuffer
import kotlin.test.Test
import kotlin.test.assertEquals

class HandshakePacketTest {
    @Test
    fun roundTripsLoginIntent() {
        val packet = HandshakePacket(776, "play.basaltmc.org", 25565, ClientIntent.LOGIN)
        val buffer = ByteBuffer()
        packet.write(buffer)
        assertEquals(packet, HandshakePacket.read(buffer))
    }

    @Test
    fun roundTripsStatusIntent() {
        val packet = HandshakePacket(776, "127.0.0.1", 25565, ClientIntent.STATUS)
        val buffer = ByteBuffer()
        packet.write(buffer)
        assertEquals(packet, HandshakePacket.read(buffer))
    }

    @Test
    fun clientIntentIdsMatchVanilla() {
        assertEquals(1, ClientIntent.STATUS.id)
        assertEquals(2, ClientIntent.LOGIN.id)
        assertEquals(3, ClientIntent.TRANSFER.id)
    }
}
