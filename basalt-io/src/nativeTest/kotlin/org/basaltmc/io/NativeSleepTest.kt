package org.basaltmc.io

import kotlin.test.Test
import kotlin.test.assertTrue
import kotlin.time.TimeSource

class NativeSleepTest {
    @Test
    fun sleepsForAtLeastTheRequestedDuration() {
        val start = TimeSource.Monotonic.markNow()
        sleepMillis(50)
        assertTrue(start.elapsedNow().inWholeMilliseconds >= 45, "slept for less than requested")
    }

    @Test
    fun toleratesZero() {
        sleepMillis(0)
    }
}
