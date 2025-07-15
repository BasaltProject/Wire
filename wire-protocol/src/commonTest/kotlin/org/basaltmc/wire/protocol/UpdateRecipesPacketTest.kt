package org.basaltmc.wire.protocol

import org.basaltmc.wire.io.ByteBuffer
import org.basaltmc.wire.protocol.play.StonecutterOption
import org.basaltmc.wire.protocol.play.UpdateRecipesPacket
import kotlin.test.Test
import kotlin.test.assertEquals

class UpdateRecipesPacketTest {
    @Test
    fun `the stonecutter's list survives a round trip`() {
        val packet =
            UpdateRecipesPacket(
                listOf(
                    StonecutterOption(inputItemIds = listOf(6), resultItemId = 736, resultCount = 2),
                    StonecutterOption(inputItemIds = listOf(1416, 1417), resultItemId = 494, resultCount = 1),
                ),
            )
        val buffer = ByteBuffer().also { packet.write(it) }
        assertEquals(packet, UpdateRecipesPacket.read(buffer))
        assertEquals(0, buffer.readableBytes, "the packet ends where it says it does")
    }
}
