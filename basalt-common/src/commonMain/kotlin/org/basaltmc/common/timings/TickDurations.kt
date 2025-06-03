package org.basaltmc.common.timings

import kotlin.concurrent.AtomicLong

/** min, median, 95th and max of a set of samples, in milliseconds. */
public data class DurationSummary(
    public val samples: Int,
    public val minMillis: Double,
    public val medianMillis: Double,
    public val percentile95Millis: Double,
    public val maxMillis: Double,
    public val meanMillis: Double,
) {
    public companion object {
        public val EMPTY: DurationSummary = DurationSummary(0, 0.0, 0.0, 0.0, 0.0, 0.0)
    }
}

/**
 * How long each tick took, for the last few minutes. TPS is a rate and hides this: a server pinned
 * at 20 TPS with one tick in fifty taking 80ms stutters. Hence min/median/95th/max rather than an
 * average, which the very outliers being looked for drag around.
 *
 * A fixed-size ring with no lock, written by the tick thread: a reader catching a sample mid-write
 * gets one wrong figure in thousands, and locking every tick would be measuring the measurement.
 */
public class TickDurations(
    private val capacity: Int = DEFAULT_CAPACITY,
) {
    private val durations = LongArray(capacity)
    private val timestamps = LongArray(capacity)
    private val written = AtomicLong(0)

    public fun record(
        atNanos: Long,
        durationNanos: Long,
    ) {
        val index = (written.value % capacity).toInt()
        durations[index] = durationNanos
        timestamps[index] = atNanos
        written.addAndGet(1)
    }

    /**
     * Ticks counted in the last [windowSeconds], divided by how much of that window actually exists.
     *
     * The division is by observed time, not by the window: a server thirty seconds old has no last
     * five minutes, and dividing its ticks by three hundred reported 0.14 TPS on a server running
     * perfectly. Until a window has filled, its rate is the rate over however long there has been.
     */
    public fun ticksPerSecond(
        nowNanos: Long,
        windowSeconds: Int,
    ): Double {
        val cutoff = nowNanos - windowSeconds.toLong() * NANOS_PER_SECOND
        var counted = 0
        var oldest = Long.MAX_VALUE
        forEachSampleAt(cutoff) { at, _ ->
            counted++
            if (at < oldest) oldest = at
        }
        if (counted == 0) return 0.0
        val observedNanos = (nowNanos - oldest).coerceAtLeast(NANOS_PER_TICK)
        val seconds = observedNanos.toDouble() / NANOS_PER_SECOND
        return (counted.toDouble() / seconds).coerceAtMost(MAX_TICKS_PER_SECOND)
    }

    public fun summary(
        nowNanos: Long,
        windowSeconds: Int,
    ): DurationSummary {
        val cutoff = nowNanos - windowSeconds.toLong() * NANOS_PER_SECOND
        val collected = mutableListOf<Long>()
        forEachSampleAt(cutoff) { _, duration -> collected.add(duration) }
        if (collected.isEmpty()) return DurationSummary.EMPTY

        collected.sort()
        val toMillis = { nanos: Long -> nanos / 1_000_000.0 }
        return DurationSummary(
            samples = collected.size,
            minMillis = toMillis(collected.first()),
            medianMillis = toMillis(percentile(collected, 0.50)),
            percentile95Millis = toMillis(percentile(collected, 0.95)),
            maxMillis = toMillis(collected.last()),
            meanMillis = toMillis(collected.sum() / collected.size),
        )
    }

    public fun reset() {
        written.value = 0
        durations.fill(0)
        timestamps.fill(0)
    }

    private inline fun forEachSampleAt(
        cutoffNanos: Long,
        action: (Long, Long) -> Unit,
    ) {
        val total = written.value
        val available = minOf(total, capacity.toLong()).toInt()
        for (offset in 0 until available) {
            val index = ((total - 1 - offset) % capacity).toInt()
            if (timestamps[index] < cutoffNanos) continue
            action(timestamps[index], durations[index])
        }
    }

    private fun percentile(
        sorted: List<Long>,
        fraction: Double,
    ): Long {
        // Nearest-rank, which is what every server tool means by "the 95th percentile tick": the
        // value an actual tick had, not one interpolated between two that never happened.
        val rank = (fraction * sorted.size).toInt().coerceIn(0, sorted.size - 1)
        return sorted[rank]
    }

    public companion object {
        public const val NANOS_PER_SECOND: Long = 1_000_000_000L

        /** Five minutes at twenty a second, which is the longest window anything asks for. */
        public const val DEFAULT_CAPACITY: Int = 20 * 60 * 5

        private const val MAX_TICKS_PER_SECOND = 20.0

        /** One tick, the smallest span a rate can honestly be measured over. */
        private const val NANOS_PER_TICK = 50_000_000L
    }
}
