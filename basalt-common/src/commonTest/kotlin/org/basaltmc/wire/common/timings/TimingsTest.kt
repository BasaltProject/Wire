package org.basaltmc.wire.common.timings

import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertSame
import kotlin.test.assertTrue

class TimingsTest {
    @Test
    fun nothingIsMeasuredUntilItIsSwitchedOn() {
        val timings = Timings()
        val section = timings.of("tick")

        var ran = 0
        section.time { ran++ }

        // The block still runs; only the measuring is skipped, which is what makes leaving the
        // instrumentation in the hot path acceptable.
        assertEquals(1, ran)
        assertEquals(0, section.snapshot().calls)
    }

    @Test
    fun onceOnItCountsCallsTotalAndWorstCase() {
        val timings = Timings().apply { enabled = true }
        val section = timings.of("tick")

        section.record(1_000_000)
        section.record(5_000_000)
        section.record(2_000_000)

        val snapshot = section.snapshot()
        assertEquals(3, snapshot.calls)
        assertEquals(8_000_000, snapshot.totalNanos)
        // The worst case is the whole point: an average hides the one tick that stuttered.
        assertEquals(5_000_000, snapshot.maxNanos)
        assertEquals(8_000_000 / 3, snapshot.averageNanos)
    }

    @Test
    fun theSameNameIsAlwaysTheSameTimer() {
        val timings = Timings()

        assertSame(timings.of("world.save"), timings.of("world.save"))
        assertEquals(setOf("world.save"), timings.names)
    }

    @Test
    fun resettingClearsEveryCounter() {
        val timings = Timings().apply { enabled = true }
        timings.of("a").record(10)
        timings.of("b").record(20)

        timings.reset(nowNanos = 0)

        assertTrue(timings.snapshot().all { it.calls == 0L && it.totalNanos == 0L })
    }

    @Test
    fun aBlockThatThrowsIsStillMeasured() {
        val timings = Timings().apply { enabled = true }
        val section = timings.of("risky")

        runCatching { section.time { throw IllegalStateException("boom") } }

        // Measured in a finally: the calls that fail are the ones worth knowing the cost of.
        assertEquals(1, section.snapshot().calls)
    }
}

class TickDurationsTest {
    private val second = TickDurations.NANOS_PER_SECOND

    @Test
    fun theRateIsWhatWasCountedInTheWindow() {
        val durations = TickDurations()
        // Twenty ticks in the last second, one per 50ms.
        for (index in 0 until 20) durations.record(atNanos = index * second / 20, durationNanos = 1_000_000)

        assertEquals(20.0, durations.ticksPerSecond(nowNanos = second, windowSeconds = 1))
    }

    @Test
    fun theDistributionIsWhatABareRateHides() {
        val durations = TickDurations()
        // Forty-nine quick ticks and one that took 80ms: a server at full rate that still stutters.
        for (index in 0 until 49) durations.record(index * 1_000_000L, 1_000_000)
        durations.record(49_000_000L, 80_000_000)

        val summary = durations.summary(nowNanos = 50_000_000L, windowSeconds = 10)
        assertEquals(50, summary.samples)
        assertEquals(1.0, summary.minMillis)
        assertEquals(1.0, summary.medianMillis)
        assertEquals(80.0, summary.maxMillis)
        // The mean is dragged by the outlier, which is exactly why the report shows both.
        assertTrue(summary.meanMillis > summary.medianMillis)
    }

    @Test
    fun samplesOlderThanTheWindowAreLeftOut() {
        val durations = TickDurations()
        durations.record(atNanos = 0, durationNanos = 90_000_000)
        durations.record(atNanos = 30 * second, durationNanos = 1_000_000)

        val summary = durations.summary(nowNanos = 30 * second, windowSeconds = 10)

        assertEquals(1, summary.samples)
        assertEquals(1.0, summary.maxMillis)
    }

    @Test
    fun anEmptyWindowSaysNothingRatherThanZero() {
        val durations = TickDurations()

        assertEquals(0, durations.summary(nowNanos = 10 * second, windowSeconds = 10).samples)
        assertEquals(0.0, durations.ticksPerSecond(nowNanos = 10 * second, windowSeconds = 10))
    }

    @Test
    fun theRingKeepsTheMostRecentSamplesWhenItWrapsAround() {
        val durations = TickDurations(capacity = 4)
        for (index in 0 until 6) durations.record(atNanos = index.toLong(), durationNanos = index * 1_000_000L)

        val summary = durations.summary(nowNanos = 6, windowSeconds = 10)
        assertEquals(4, summary.samples)
        // The first two were overwritten, so the smallest left is the third.
        assertEquals(2.0, summary.minMillis)
        assertEquals(5.0, summary.maxMillis)
    }
}
