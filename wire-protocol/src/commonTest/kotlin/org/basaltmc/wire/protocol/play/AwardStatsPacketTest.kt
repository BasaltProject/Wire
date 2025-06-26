package org.basaltmc.wire.protocol.play

import org.basaltmc.wire.io.ByteBuffer
import kotlin.test.Test
import kotlin.test.assertEquals

/** parity: `ClientboundAwardStatsPacket`, whose map is written as size then key then value. */
class AwardStatsPacketTest {
    @Test
    fun itRoundTripsEveryCounter() {
        val buffer = ByteBuffer()
        val packet =
            AwardStatsPacket(
                listOf(
                    StatCounter(AwardStatsPacket.TYPE_CUSTOM, 32, 4),
                    StatCounter(AwardStatsPacket.TYPE_MINED, 1, 128),
                ),
            )
        packet.write(buffer)
        assertEquals(packet, AwardStatsPacket.read(buffer))
    }
}
