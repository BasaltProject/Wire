package org.basaltmc.wire.protocol.play

import org.basaltmc.wire.io.ByteBuffer
import kotlin.math.abs
import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertTrue

/** Mirrors `LpVec3`, the quantised vector 26.2 introduced. */
class LowPrecisionVec3Test {
    private fun roundTrip(
        x: Double,
        y: Double,
        z: Double,
    ): DoubleArray {
        val buffer = ByteBuffer()
        LowPrecisionVec3.write(buffer, x, y, z)
        return LowPrecisionVec3.read(buffer)
    }

    /** Nothing at all is one zero byte, which is the case the format is tuned for. */
    @Test
    fun nothingCostsOneByte() {
        val buffer = ByteBuffer()
        LowPrecisionVec3.write(buffer, 0.0, 0.0, 0.0)

        assertEquals(1, buffer.readableBytes)
        val read = LowPrecisionVec3.read(buffer)
        assertEquals(0.0, read[0])
        assertEquals(0.0, read[1])
        assertEquals(0.0, read[2])
    }

    /** A point on an entity survives the round trip to within the precision the format has. */
    @Test
    fun aPointOnAnEntityComesBack() {
        val read = roundTrip(0.3, 1.2, -0.45)

        assertTrue(abs(read[0] - 0.3) < 1e-3, "x came back as ${read[0]}")
        assertTrue(abs(read[1] - 1.2) < 1e-3, "y came back as ${read[1]}")
        assertTrue(abs(read[2] + 0.45) < 1e-3, "z came back as ${read[2]}")
    }

    /** Six bytes while the scale fits in two bits, and a varint after it when it does not. */
    @Test
    fun theScaleGrowsWithTheVector() {
        val small = ByteBuffer()
        LowPrecisionVec3.write(small, 1.0, 2.0, 3.0)
        assertEquals(6, small.readableBytes, "a short vector is six bytes")

        val large = ByteBuffer()
        LowPrecisionVec3.write(large, 100.0, 0.0, 0.0)
        assertTrue(large.readableBytes > 6, "a long one carries the rest of its scale")

        val read = LowPrecisionVec3.read(large)
        assertTrue(abs(read[0] - 100.0) < 0.1, "and still comes back: ${read[0]}")
    }

    @Test
    fun theWholeInteractPacketRoundTrips() {
        val buffer = ByteBuffer()
        InteractPacket(
            entityId = 42,
            hand = InteractPacket.MAIN_HAND,
            x = 0.1,
            y = 0.9,
            z = -0.2,
            usingSecondaryAction = true,
        ).write(buffer)

        val read = InteractPacket.read(buffer)
        assertEquals(42, read.entityId)
        assertEquals(InteractPacket.MAIN_HAND, read.hand)
        assertTrue(read.usingSecondaryAction)
        assertTrue(abs(read.y - 0.9) < 1e-3)
    }

    @Test
    fun anAttackIsJustAnEntityId() {
        val buffer = ByteBuffer()
        AttackPacket(entityId = 7).write(buffer)

        assertEquals(7, AttackPacket.read(buffer).entityId)
    }
}
