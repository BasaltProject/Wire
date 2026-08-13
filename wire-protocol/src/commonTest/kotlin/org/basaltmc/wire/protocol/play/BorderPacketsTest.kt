package org.basaltmc.wire.protocol.play

import org.basaltmc.wire.io.ByteBuffer
import kotlin.test.Test
import kotlin.test.assertEquals

class BorderPacketsTest {
    @Test
    fun everyBorderPacketRoundTrips() {
        val initialize =
            InitializeBorderPacket(
                centerX = 10.0,
                centerZ = -20.0,
                oldSize = 100.0,
                newSize = 50.0,
                lerpTimeMillis = 5000L,
                absoluteMaxSize = InitializeBorderPacket.ABSOLUTE_MAX_SIZE,
                warningBlocks = 5,
                warningSeconds = 15,
            )
        val initializeBuffer = ByteBuffer()
        initialize.write(initializeBuffer)
        assertEquals(initialize, InitializeBorderPacket.read(initializeBuffer))

        val centerBuffer = ByteBuffer()
        val center = SetBorderCenterPacket(1.5, -2.5)
        center.write(centerBuffer)
        assertEquals(center, SetBorderCenterPacket.read(centerBuffer))

        val sizeBuffer = ByteBuffer()
        val size = SetBorderSizePacket(64.0)
        size.write(sizeBuffer)
        assertEquals(size, SetBorderSizePacket.read(sizeBuffer))

        val lerpBuffer = ByteBuffer()
        val lerp = SetBorderLerpSizePacket(64.0, 32.0, 1000L)
        lerp.write(lerpBuffer)
        assertEquals(lerp, SetBorderLerpSizePacket.read(lerpBuffer))

        val delayBuffer = ByteBuffer()
        val delay = SetBorderWarningDelayPacket(10)
        delay.write(delayBuffer)
        assertEquals(delay, SetBorderWarningDelayPacket.read(delayBuffer))

        val distanceBuffer = ByteBuffer()
        val distance = SetBorderWarningDistancePacket(4)
        distance.write(distanceBuffer)
        assertEquals(distance, SetBorderWarningDistancePacket.read(distanceBuffer))
    }

    @Test
    fun cameraAndTickingRoundTrip() {
        val cameraBuffer = ByteBuffer()
        val camera = SetCameraPacket(42)
        camera.write(cameraBuffer)
        assertEquals(camera, SetCameraPacket.read(cameraBuffer))

        val stateBuffer = ByteBuffer()
        val state = TickingStatePacket(tickRate = 20f, frozen = true)
        state.write(stateBuffer)
        assertEquals(state, TickingStatePacket.read(stateBuffer))

        val stepBuffer = ByteBuffer()
        val step = TickingStepPacket(5)
        step.write(stepBuffer)
        assertEquals(step, TickingStepPacket.read(stepBuffer))
    }
}
