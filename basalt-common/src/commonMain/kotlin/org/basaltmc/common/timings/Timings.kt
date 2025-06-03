package org.basaltmc.common.timings

import kotlin.concurrent.AtomicLong
import kotlin.concurrent.AtomicReference
import kotlin.time.ExperimentalTime
import kotlin.time.TimeSource

/** One timer's numbers at a moment in time. */
public data class TimingSnapshot(
    public val name: String,
    public val calls: Long,
    public val totalNanos: Long,
    public val maxNanos: Long,
) {
    public val averageNanos: Long get() = if (calls == 0L) 0L else totalNanos / calls
}

/**
 * One named section of work, measured every time it runs.
 *
 * Counters are atomic because the interesting sections are exactly the ones several threads run at
 * once: chunk generation on its workers, ticks on theirs, packets on one per connection. A count
 * that occasionally lost an increment would be worse than no count at all, since the whole use of
 * this is comparing totals.
 *
 * `max` is kept and `min` is not, deliberately. The question this answers is "what made the server
 * stutter", and that is the worst case; the best case is never the reason anyone opens a report.
 */
public class Timing internal constructor(
    public val name: String,
    private val owner: Timings,
) {
    private val calls = AtomicLong(0)
    private val totalNanos = AtomicLong(0)
    private val maxNanos = AtomicLong(0)

    /** Read before measuring, so a disabled registry costs one atomic read and nothing else. */
    public val isEnabled: Boolean get() = owner.enabled

    public fun record(nanos: Long) {
        calls.addAndGet(1)
        totalNanos.addAndGet(nanos)
        while (true) {
            val worst = maxNanos.value
            if (nanos <= worst || maxNanos.compareAndSet(worst, nanos)) break
        }
    }

    public fun snapshot(): TimingSnapshot = TimingSnapshot(name, calls.value, totalNanos.value, maxNanos.value)

    public fun reset() {
        calls.value = 0
        totalNanos.value = 0
        maxNanos.value = 0
    }
}

/**
 * Every named section this server measures.
 *
 * Off by default, and that is the point of the flag rather than of a build option: measuring costs
 * a clock read per section per call, which is small but not free, and a server that is not being
 * investigated should not pay it. Turning it on and off is what `/timings` does.
 *
 * Sections are named with dots (`player.tick`), which is how the report nests them. Nothing here
 * enforces that a parent exists: a name is a label, and inventing a hierarchy that has to be kept
 * in step with the code is how these things rot.
 */
public class Timings {
    private val running = AtomicLong(0)
    private val registry = AtomicReference<Map<String, Timing>>(emptyMap())

    /** When this became true, so a report can say what window its numbers cover. */
    private val startedAtNanos = AtomicLong(0)

    public var enabled: Boolean
        get() = running.value == 1L
        set(value) {
            running.value = if (value) 1L else 0L
        }

    public val names: Set<String> get() = registry.value.keys

    /**
     * The timer for [name], creating it the first time.
     *
     * Compare-and-set rather than a lock: creation happens a handful of times at startup, and a lock
     * here would be held on the hot path by every thread that ever measures anything.
     */
    public fun of(name: String): Timing {
        registry.value[name]?.let { return it }
        while (true) {
            val current = registry.value
            current[name]?.let { return it }
            val created = Timing(name, this)
            if (registry.compareAndSet(current, current + (name to created))) return created
        }
    }

    public fun snapshot(): List<TimingSnapshot> =
        registry.value.values
            .map { it.snapshot() }
            .sortedBy { it.name }

    /** Clears every counter and restarts the window. Called when measuring is switched on. */
    public fun reset(nowNanos: Long) {
        for (timing in registry.value.values) timing.reset()
        startedAtNanos.value = nowNanos
    }

    public fun windowNanos(nowNanos: Long): Long = (nowNanos - startedAtNanos.value).coerceAtLeast(0)
}

/**
 * Measures [block] and returns what it returned.
 *
 * Inline and guarded so that a disabled timing adds one atomic read to the call: the sections worth
 * measuring are inside the tick loop, and instrumentation that costs something when switched off is
 * instrumentation that gets removed again.
 */
@OptIn(ExperimentalTime::class)
public inline fun <T> Timing.time(block: () -> T): T {
    if (!isEnabled) return block()
    val started = TimeSource.Monotonic.markNow()
    try {
        return block()
    } finally {
        record(started.elapsedNow().inWholeNanoseconds)
    }
}
