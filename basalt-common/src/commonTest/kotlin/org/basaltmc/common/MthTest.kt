package org.basaltmc.common

import kotlin.test.Test
import kotlin.test.assertEquals

class MthTest {
    @Test
    fun floorRoundsTowardNegativeInfinity() {
        assertEquals(3, Mth.floor(3.7))
        assertEquals(-4, Mth.floor(-3.2))
        assertEquals(0, Mth.floor(0.0))
        assertEquals(-1, Mth.floor(-0.0001))
    }

    @Test
    fun lfloorMatchesFloorAsALong() {
        assertEquals(3L, Mth.lfloor(3.7))
        assertEquals(-4L, Mth.lfloor(-3.2))
    }

    @Test
    fun sineTableMatchesVanillaFloatBits() {
        assertEquals(1062693212, Mth.sin(1.0).toBits())
        assertEquals(1057641281, Mth.cos(1.0).toBits())
        assertEquals(3210176860u, Mth.sin(-1.0).toBits().toUInt())
    }

    @Test
    fun lerp3IsTrilinearInterpolation() {
        val value = Mth.lerp3(0.5, 0.5, 0.5, 0.0, 1.0, 0.0, 1.0, 0.0, 1.0, 0.0, 1.0)
        assertEquals(0.5, value, 1.0E-12)
    }

    @Test
    fun smoothstepIsZeroAtZeroAndOneAtOne() {
        assertEquals(0.0, Mth.smoothstep(0.0), 1.0E-12)
        assertEquals(1.0, Mth.smoothstep(1.0), 1.0E-12)
    }

    @Test
    fun fastInverseSquareRootMatchesVanillaBits() {
        assertEquals(4613549875023816620L, Mth.fastInvSqrt(0.125).toRawBits())
        assertEquals(4609046275396446124L, Mth.fastInvSqrt(0.5).toRawBits())
        assertEquals(4607167179904312135L, Mth.fastInvSqrt(1.0).toRawBits())
        assertEquals(4604542675769075628L, Mth.fastInvSqrt(2.0).toRawBits())
        assertEquals(4597901004127826181L, Mth.fastInvSqrt(17.0).toRawBits())
    }
}
