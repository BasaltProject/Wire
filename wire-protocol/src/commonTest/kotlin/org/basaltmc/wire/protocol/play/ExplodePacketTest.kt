package org.basaltmc.wire.protocol.play

import org.basaltmc.wire.io.ByteBuffer
import kotlin.test.Test
import kotlin.test.assertEquals

class ExplodePacketTest {
    @Test
    fun `an explosion survives a round trip`() {
        val packet =
            ExplodePacket(
                x = 1.5,
                y = 64.0,
                z = -3.25,
                radius = 4f,
                blockCount = 12,
                knockbackX = 0.5,
                knockbackY = 0.25,
                knockbackZ = -0.5,
                particleId = 20,
                sound = "minecraft:entity.generic.explode",
                blockParticles =
                    listOf(
                        ExplosionParticle(11, ExplodePacket.POOF_SCALING, 1f, 1),
                        ExplosionParticle(12, ExplodePacket.SMOKE_SCALING, 1f, 1),
                    ),
            )
        val buffer = ByteBuffer().also { packet.write(it) }
        assertEquals(packet, ExplodePacket.read(buffer))
        assertEquals(0, buffer.readableBytes)
    }

    @Test
    fun `an explosion that pushes nobody survives too`() {
        val packet =
            ExplodePacket(
                x = 0.0,
                y = 0.0,
                z = 0.0,
                radius = 1f,
                blockCount = 0,
                particleId = 3,
                sound = "minecraft:entity.generic.explode",
            )
        val buffer = ByteBuffer().also { packet.write(it) }
        assertEquals(packet, ExplodePacket.read(buffer))
        assertEquals(0, buffer.readableBytes)
    }
}
