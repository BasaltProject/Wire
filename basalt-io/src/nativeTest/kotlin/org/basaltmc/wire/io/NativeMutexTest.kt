package org.basaltmc.wire.io

import kotlin.concurrent.AtomicInt
import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.time.TimeSource

class NativeMutexTest {
    @Test
    fun serializesConcurrentIncrementsOfAPlainCounter() {
        val mutex = NativeMutex()
        var counter = 0 // deliberately not an AtomicInt: this is what the mutex is meant to protect
        val threadCount = 200
        val incrementsPerThread = 500
        val done = AtomicInt(0)

        repeat(threadCount) {
            NativeThread.start {
                repeat(incrementsPerThread) {
                    mutex.withLock { counter++ }
                }
                done.addAndGet(1)
            }
        }

        val deadline = TimeSource.Monotonic.markNow()
        while (done.value < threadCount && deadline.elapsedNow().inWholeSeconds < 30) {
            // busy-wait for every thread to finish
        }

        assertEquals(threadCount, done.value, "not every thread finished")
        assertEquals(threadCount * incrementsPerThread, counter)
        mutex.destroy()
    }
}
