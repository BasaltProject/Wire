package org.basaltmc.wire.protocol.play

import org.basaltmc.wire.io.ByteBuffer
import kotlin.test.Test
import kotlin.test.assertEquals

/** parity: `ClientboundCooldownPacket`, a group name and the ticks left on it. */
class CooldownPacketTest {
    @Test
    fun itRoundTrips() {
        val buffer = ByteBuffer()
        val packet = CooldownPacket("minecraft:ender_pearl", 20)
        packet.write(buffer)
        assertEquals(packet, CooldownPacket.read(buffer))
    }
}
